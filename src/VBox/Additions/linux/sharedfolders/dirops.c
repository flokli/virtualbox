/* $Id$ */
/** @file
 * vboxsf - VBox Linux Shared Folders VFS, directory inode and file operations.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "vfsmod.h"
#include <iprt/err.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
# define d_in_lookup(a_pDirEntry)  (d_unhashed(a_pDirEntry))
#endif



/**
 * Open a directory (implements file_operations::open).
 *
 * @returns 0 on success, negative errno otherwise.
 * @param   inode   inode
 * @param   file    file
 */
static int vbsf_dir_open(struct inode *inode, struct file *file)
{
    struct vbsf_super_info *sf_g   = VBSF_GET_SUPER_INFO(inode->i_sb);
    struct vbsf_inode_info *sf_i   = VBSF_GET_INODE_INFO(inode);
    struct dentry          *dentry = VBSF_GET_F_DENTRY(file);
    struct vbsf_dir_info   *sf_d;
    int                     rc;

    SFLOGFLOW(("vbsf_dir_open: inode=%p file=%p %s\n", inode, file, sf_i && sf_i->path ? sf_i->path->String.ach : NULL));
    AssertReturn(sf_g, -EINVAL);
    AssertReturn(sf_i, -EINVAL);
    AssertReturn(!file->private_data, 0);

    /*
     * Allocate and initialize our directory info structure.
     * We delay buffer allocation until vbsf_getdent is actually used.
     */
    sf_d = kmalloc(sizeof(*sf_d), GFP_KERNEL);
    if (sf_d) {
        VBOXSFCREATEREQ *pReq;
        RT_ZERO(*sf_d);
        sf_d->u32Magic = VBSF_DIR_INFO_MAGIC;
        sema_init(&sf_d->Lock, 1);

        /*
         * Try open the directory.
         */
        pReq = (VBOXSFCREATEREQ *)VbglR0PhysHeapAlloc(RT_UOFFSETOF(VBOXSFCREATEREQ, StrPath.String) + sf_i->path->u16Size);
        if (pReq) {
            memcpy(&pReq->StrPath, sf_i->path, SHFLSTRING_HEADER_SIZE + sf_i->path->u16Size);
            RT_ZERO(pReq->CreateParms);
            pReq->CreateParms.Handle      = SHFL_HANDLE_NIL;
            pReq->CreateParms.CreateFlags = SHFL_CF_DIRECTORY
                                          | SHFL_CF_ACT_OPEN_IF_EXISTS
                                          | SHFL_CF_ACT_FAIL_IF_NEW
                                          | SHFL_CF_ACCESS_READ;

            LogFunc(("calling VbglR0SfHostReqCreate on folder %s, flags %#x\n",
                     sf_i->path->String.utf8, pReq->CreateParms.CreateFlags));
            rc = VbglR0SfHostReqCreate(sf_g->map.root, pReq);
            if (RT_SUCCESS(rc)) {
                if (pReq->CreateParms.Result == SHFL_FILE_EXISTS) {
                    Assert(pReq->CreateParms.Handle != SHFL_HANDLE_NIL);

                    /*
                     * Update the inode info with fresh stats and increase the TTL for the
                     * dentry cache chain that got us here.
                     */
                    vbsf_update_inode(inode, sf_i, &pReq->CreateParms.Info, sf_g, true /*fLocked*/ /** @todo inode locking */);
                    vbsf_dentry_chain_increase_ttl(dentry);

                    sf_d->Handle.hHost  = pReq->CreateParms.Handle;
                    sf_d->Handle.cRefs  = 1;
                    sf_d->Handle.fFlags = VBSF_HANDLE_F_READ | VBSF_HANDLE_F_DIR | VBSF_HANDLE_F_MAGIC;
                    vbsf_handle_append(sf_i, &sf_d->Handle);

                    file->private_data = sf_d;
                    VbglR0PhysHeapFree(pReq);
                    SFLOGFLOW(("vbsf_dir_open(%p,%p): returns 0; hHost=%#llx\n", inode, file, sf_d->Handle.hHost));
                    return 0;

                }
                Assert(pReq->CreateParms.Handle == SHFL_HANDLE_NIL);

                /*
                 * Directory does not exist, so we probably got some invalid
                 * dir cache and inode info.
                 */
                /** @todo do more to invalidate dentry and inode here. */
                vbsf_dentry_set_update_jiffies(dentry, jiffies + INT_MAX / 2);
                sf_i->force_restat = true;
                rc = -ENOENT;
            } else
                rc = -EPERM;
            VbglR0PhysHeapFree(pReq);
        } else {
            LogRelMaxFunc(64, ("failed to allocate %zu bytes for '%s'\n",
                               RT_UOFFSETOF(VBOXSFCREATEREQ, StrPath.String) + sf_i->path->u16Size, sf_i->path->String.ach));
            rc = -ENOMEM;
        }
        sf_d->u32Magic = VBSF_DIR_INFO_MAGIC_DEAD;
        kfree(sf_d);
    } else
        rc = -ENOMEM;
    SFLOGFLOW(("vbsf_dir_open(%p,%p): returns %d\n", inode, file, rc));
    return rc;
}


/**
 * This is called when reference count of [file] goes to zero. Notify
 * the host that it can free whatever is associated with this directory
 * and deallocate our own internal buffers
 *
 * @param inode     inode
 * @param file      file
 * returns 0 on success, Linux error code otherwise
 */
static int vbsf_dir_release(struct inode *inode, struct file *file)
{
    struct vbsf_dir_info *sf_d = (struct vbsf_dir_info *)file->private_data;

    SFLOGFLOW(("vbsf_dir_release(%p,%p): sf_d=%p hHost=%#llx\n", inode, file, sf_d, sf_d ? sf_d->Handle.hHost : SHFL_HANDLE_NIL));

    if (sf_d) {
        struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(inode->i_sb);

        /* Invalidate the non-handle part. */
        sf_d->u32Magic     = VBSF_DIR_INFO_MAGIC_DEAD;
        sf_d->cEntriesLeft = 0;
        sf_d->cbValid      = 0;
        sf_d->pEntry       = NULL;
        sf_d->fNoMoreFiles = false;
        if (sf_d->pBuf) {
            kfree(sf_d->pBuf);
            sf_d->pBuf = NULL;
        }

        /* Closes the handle and frees the structure when the last reference is released. */
        vbsf_handle_release(&sf_d->Handle, sf_g, "vbsf_dir_release");
    }

    return 0;
}


/**
 * Translate RTFMODE into DT_xxx (in conjunction to rtDirType()).
 * returns d_type
 * @param  fMode    file mode
 */
DECLINLINE(int) vbsf_get_d_type(RTFMODE fMode)
{
    switch (fMode & RTFS_TYPE_MASK) {
        case RTFS_TYPE_FIFO:        return DT_FIFO;
        case RTFS_TYPE_DEV_CHAR:    return DT_CHR;
        case RTFS_TYPE_DIRECTORY:   return DT_DIR;
        case RTFS_TYPE_DEV_BLOCK:   return DT_BLK;
        case RTFS_TYPE_FILE:        return DT_REG;
        case RTFS_TYPE_SYMLINK:     return DT_LNK;
        case RTFS_TYPE_SOCKET:      return DT_SOCK;
        case RTFS_TYPE_WHITEOUT:    return DT_WHT;
    }
    return DT_UNKNOWN;
}


/**
 * Refills the buffer with more entries.
 *
 * @returns 0 on success, negative errno on error,
 */
static int vbsf_dir_read_more(struct vbsf_dir_info *sf_d, struct vbsf_super_info *sf_g, bool fRestart)
{
    int               rc;
    VBOXSFLISTDIRREQ *pReq;

    /*
     * Don't call the host again if we've reached the end of the
     * directory entries already.
     */
    if (sf_d->fNoMoreFiles) {
        if (!fRestart) {
            SFLOGFLOW(("vbsf_dir_read_more: no more files\n"));
            return 0;
        }
        sf_d->fNoMoreFiles = false;
    }

    /*
     * Make sure we've got some kind of buffers.
     */
    if (sf_d->pBuf) {
        /* Likely, except for the first time. */
    } else {
        /** @todo make the buffer size configurable. */
        sf_d->pBuf = (PSHFLDIRINFO)kmalloc(_64K, GFP_KERNEL);
        if (sf_d->pBuf)
            sf_d->cbBuf = _64K;
        else {
            sf_d->pBuf = (PSHFLDIRINFO)kmalloc(_4K, GFP_KERNEL);
            if (!sf_d->pBuf) {
                LogRelMax(10, ("vbsf_dir_read_more: Failed to allocate buffer!\n"));
                return -ENOMEM;
            }
            sf_d->cbBuf = _4K;
        }
    }

    /*
     * Allocate a request buffer.
     */
    pReq = (VBOXSFLISTDIRREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq) {
        rc = VbglR0SfHostReqListDirContig2x(sf_g->map.root, pReq, sf_d->Handle.hHost, NULL, NIL_RTGCPHYS64,
                                            fRestart ? SHFL_LIST_RESTART : SHFL_LIST_NONE,
                                            sf_d->pBuf, virt_to_phys(sf_d->pBuf), sf_d->cbBuf);
        if (RT_SUCCESS(rc)) {
            sf_d->pEntry       = sf_d->pBuf;
            sf_d->cbValid      = pReq->Parms.cb32Buffer.u.value32;
            sf_d->cEntriesLeft = pReq->Parms.c32Entries.u.value32;
            sf_d->fNoMoreFiles = pReq->Parms.f32More.u.value32 == 0;
        } else {
            sf_d->pEntry       = sf_d->pBuf;
            sf_d->cbValid      = 0;
            sf_d->cEntriesLeft = 0;
            if (rc == VERR_NO_MORE_FILES) {
                sf_d->fNoMoreFiles = true;
                rc = 0;
            } else {
                /* In theory we could end up here with a buffer overflow, but
                   with a 4KB minimum buffer size that's very unlikely with the
                   typical filename length of today's file systems (2019). */
                LogRelMax(16, ("vbsf_dir_read_more: VbglR0SfHostReqListDirContig2x -> %Rrc\n", rc));
                rc = -EPROTO;
            }
        }
        VbglR0PhysHeapFree(pReq);
    } else
        rc = -ENOMEM;
    SFLOGFLOW(("vbsf_dir_read_more: returns %d; cbValid=%#x cEntriesLeft=%#x fNoMoreFiles=%d\n",
               rc, sf_d->cbValid, sf_d->cEntriesLeft, sf_d->fNoMoreFiles));
    return rc;
}

/**
 * Helper function for when we need to convert the name, avoids wasting stack in
 * the UTF-8 code path.
 */
DECL_NO_INLINE(static, bool) vbsf_dir_emit_nls(
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
                                               struct dir_context *ctx,
# else
                                               void *opaque, filldir_t filldir, loff_t offPos,
# endif
                                               const char *pszSrcName, uint16_t cchSrcName, ino_t d_ino, int d_type,
                                               struct vbsf_super_info *sf_g)
{
    char szDstName[NAME_MAX];
    int rc = vbsf_nlscpy(sf_g, szDstName, sizeof(szDstName), pszSrcName, cchSrcName + 1);
    if (rc == 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
        return dir_emit(ctx, szDstName, strlen(szDstName), d_ino, d_type);
#else
        return filldir(opaque, szDstName, strlen(szDstName), offPos, d_ino, d_type) == 0;
#endif
    }

    /* Assuming this is a buffer overflow issue, just silently skip it. */
    SFLOGFLOW(("vbsf_dir_emit_nls: vbsf_nlscopy failed with %d for '%s'\n", rc, pszSrcName));
    return true;
}


/**
 * This is called when vfs wants to populate internal buffers with
 * directory [dir]s contents. [opaque] is an argument to the
 * [filldir]. [filldir] magically modifies it's argument - [opaque]
 * and takes following additional arguments (which i in turn get from
 * the host via vbsf_getdent):
 *
 * name : name of the entry (i must also supply it's length huh?)
 * type : type of the entry (FILE | DIR | etc) (i ellect to use DT_UNKNOWN)
 * pos : position/index of the entry
 * ino : inode number of the entry (i fake those)
 *
 * [dir] contains:
 * f_pos : cursor into the directory listing
 * private_data : mean of communication with the host side
 *
 * Extract elements from the directory listing (incrementing f_pos
 * along the way) and feed them to [filldir] until:
 *
 * a. there are no more entries (i.e. vbsf_getdent set done to 1)
 * b. failure to compute fake inode number
 * c. filldir returns an error (see comment on that)
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
static int vbsf_dir_iterate(struct file *dir, struct dir_context *ctx)
#else
static int vbsf_dir_read(struct file *dir, void *opaque, filldir_t filldir)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
    loff_t                  offPos = ctx->pos;
#else
    loff_t                  offPos = dir->f_pos;
#endif
    struct vbsf_dir_info   *sf_d   = (struct vbsf_dir_info *)dir->private_data;
    struct vbsf_super_info *sf_g   = VBSF_GET_SUPER_INFO(VBSF_GET_F_DENTRY(dir)->d_sb);
    int                     rc;

    /*
     * Lock the directory info structures.
     */
    if (RT_LIKELY(down_interruptible(&sf_d->Lock) == 0)) {
        /* likely */
    } else
        return -ERESTARTSYS;

    /*
     * Any seek performed in the mean time?
     */
    if (offPos == sf_d->offPos) {
        /* likely */
    } else {
        /* Restart the search if iPos is lower than the current buffer position. */
        loff_t offCurEntry = sf_d->offPos;
        if (offPos < offCurEntry) {
            rc = vbsf_dir_read_more(sf_d, sf_g, true /*fRestart*/);
            if (rc == 0)
                offCurEntry = 0;
            else {
                up(&sf_d->Lock);
                return rc;
            }
        }

        /* Skip ahead to offPos. */
        while (offCurEntry < offPos) {
            uint32_t cEntriesLeft = sf_d->cEntriesLeft;
            if ((uint64_t)(offPos - offCurEntry) >= cEntriesLeft) {
                /* Skip the current buffer and read the next: */
                offCurEntry       += cEntriesLeft;
                sf_d->offPos       = offCurEntry;
                sf_d->cEntriesLeft = 0;
                rc = vbsf_dir_read_more(sf_d, sf_g, false /*fRestart*/);
                if (rc != 0 || sf_d->cEntriesLeft == 0) {
                    up(&sf_d->Lock);
                    return rc;
                }
            } else {
                do
                {
                    PSHFLDIRINFO pEntry = sf_d->pEntry;
                    pEntry = (PSHFLDIRINFO)&pEntry->name.String.utf8[pEntry->name.u16Length];
                    AssertLogRelBreakStmt(   cEntriesLeft == 1
                                          ||    (uintptr_t)pEntry - (uintptr_t)sf_d->pBuf
                                             <= sf_d->cbValid - RT_UOFFSETOF(SHFLDIRINFO, name.String),
                                          sf_d->cEntriesLeft = 0);
                    sf_d->cEntriesLeft  = --cEntriesLeft;
                    sf_d->offPos        = ++offCurEntry;
                } while (offPos < sf_d->offPos);
            }
        }
    }

    /*
     * Handle '.' and '..' specially so we get the inode numbers right.
     * We'll skip any '.' or '..' returned by the host (included in pos,
     * however, to simplify the above skipping code).
     */
    if (offPos < 2) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
        if (offPos == 0) {
            if (dir_emit_dot(dir, ctx))
                dir->f_pos = ctx->pos = sf_d->offPos = offPos = 1;
            else {
                up(&sf_d->Lock);
                return 0;
            }
        }
        if (offPos == 1) {
            if (dir_emit_dotdot(dir, ctx))
                dir->f_pos = ctx->pos = sf_d->offPos = offPos = 2;
            else {
                up(&sf_d->Lock);
                return 0;
            }
        }
#else
        if (offPos == 0) {
            rc = filldir(opaque, ".", 1, 0, VBSF_GET_F_DENTRY(dir)->d_inode->i_ino, DT_DIR);
            if (!rc)
                dir->f_pos = sf_d->offPos = offPos = 1;
            else {
                up(&sf_d->Lock);
                return 0;
            }
        }
        if (offPos == 1) {
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 5)
            rc = filldir(opaque, "..", 2, 1, parent_ino(VBSF_GET_F_DENTRY(dir)), DT_DIR);
# else
            rc = filldir(opaque, "..", 2, 1, VBSF_GET_F_DENTRY(dir)->d_parent->d_inode->i_ino, DT_DIR);
# endif
            if (!rc)
                dir->f_pos = sf_d->offPos = offPos = 2;
            else {
                up(&sf_d->Lock);
                return 0;
            }
        }
#endif
    }

    /*
     * Produce stuff.
     */
    Assert(offPos == sf_d->offPos);
    for (;;) {
        PSHFLDIRINFO pBuf;
        PSHFLDIRINFO pEntry;

        /*
         * Do we need to read more?
         */
        uint32_t cbValid      = sf_d->cbValid;
        uint32_t cEntriesLeft = sf_d->cEntriesLeft;
        if (!cEntriesLeft) {
            rc = vbsf_dir_read_more(sf_d, sf_g, false /*fRestart*/);
            if (rc == 0) {
                cEntriesLeft = sf_d->cEntriesLeft;
                if (!cEntriesLeft) {
                    up(&sf_d->Lock);
                    return 0;
                }
                cbValid = sf_d->cbValid;
            } else {
                up(&sf_d->Lock);
                return rc;
            }
        }

        /*
         * Feed entries to the caller.
         */
        pBuf   = sf_d->pBuf;
        pEntry = sf_d->pEntry;
        do {
            /*
             * Validate the entry in case the host is messing with us.
             * We're ASSUMING the host gives us a zero terminated string (UTF-8) here.
             */
            uintptr_t const offEntryInBuf = (uintptr_t)pEntry - (uintptr_t)pBuf;
            uint16_t        cbSrcName;
            uint16_t        cchSrcName;
            AssertLogRelMsgBreak(offEntryInBuf + RT_UOFFSETOF(SHFLDIRINFO, name.String) <= cbValid,
                                 ("%#llx + %#x vs %#x\n", offEntryInBuf, RT_UOFFSETOF(SHFLDIRINFO, name.String), cbValid));
            cbSrcName  = pEntry->name.u16Size;
            cchSrcName = pEntry->name.u16Length;
            AssertLogRelBreak(offEntryInBuf + RT_UOFFSETOF(SHFLDIRINFO, name.String) + cbSrcName <= cbValid);
            AssertLogRelBreak(cchSrcName < cbSrcName);
            AssertLogRelBreak(pEntry->name.String.ach[cchSrcName] == '\0');

            /*
             * Filter out '.' and '..' entires.
             */
            if (   cchSrcName > 2
                || pEntry->name.String.ach[0] != '.'
                || (   cchSrcName == 2
                    && pEntry->name.String.ach[1] != '.')) {
                int const   d_type = vbsf_get_d_type(pEntry->Info.Attr.fMode);
                ino_t const d_ino  = (ino_t)offPos + 0xbeef; /* very fake */
                bool        fContinue;
                if (sf_g->fNlsIsUtf8) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
                    fContinue = dir_emit(ctx, pEntry->name.String.ach, cchSrcName, d_ino, d_type);
#else
                    fContinue = filldir(opaque, pEntry->name.String.ach, cchSrcName, offPos, d_ino, d_type) == 0;
#endif
                } else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
                    fContinue = vbsf_dir_emit_nls(ctx, pEntry->name.String.ach, cchSrcName, d_ino, d_type, sf_g);
#else
                    fContinue = vbsf_dir_emit_nls(opaque, filldir, offPos, pEntry->name.String.ach, cchSrcName,
                                                  d_ino, d_type, sf_g);
#endif
                }
                if (fContinue) {
                    /* likely */
                } else  {
                    sf_d->cEntriesLeft = cEntriesLeft;
                    sf_d->pEntry       = pEntry;
                    sf_d->offPos       = offPos;
                    up(&sf_d->Lock);
                    return 0;
                }
            }

            /*
             * Advance to the next entry.
             */
            pEntry        = (PSHFLDIRINFO)((uintptr_t)pEntry + RT_UOFFSETOF(SHFLDIRINFO, name.String) + cbSrcName);
            offPos       += 1;
            dir->f_pos    = offPos;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
            ctx->pos      = offPos;
#endif
            cEntriesLeft -= 1;
        } while (cEntriesLeft > 0);

        /* Done with all available entries. */
        sf_d->offPos       = offPos + cEntriesLeft;
        sf_d->pEntry       = pBuf;
        sf_d->cEntriesLeft = 0;
    }
}

/**
 * Directory file operations.
 */
struct file_operations vbsf_dir_fops = {
    .open = vbsf_dir_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
    .iterate_shared = vbsf_dir_iterate,
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
    .iterate = vbsf_dir_iterate,
#else
    .readdir = vbsf_dir_read,
#endif
    .release = vbsf_dir_release,
    .read = generic_read_dir
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
    , .llseek = generic_file_llseek
#endif
};



/*********************************************************************************************************************************
*   Directory Inode Operations                                                                                                   *
*********************************************************************************************************************************/

/**
 * Worker for vbsf_inode_lookup(), vbsf_create_worker() and
 * vbsf_inode_instantiate().
 */
static struct inode *vbsf_create_inode(struct inode *parent, struct dentry *dentry, PSHFLSTRING path,
                                       PSHFLFSOBJINFO pObjInfo, struct vbsf_super_info *sf_g, bool fInstantiate)
{
    /*
     * Allocate memory for our additional inode info and create an inode.
     */
    struct vbsf_inode_info *sf_new_i = (struct vbsf_inode_info *)kmalloc(sizeof(*sf_new_i), GFP_KERNEL);
    if (sf_new_i) {
        ino_t         iNodeNo = iunique(parent->i_sb, 16);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 25)
        struct inode *pInode  = iget_locked(parent->i_sb, iNodeNo);
#else
        struct inode *pInode  = iget(parent->i_sb, iNodeNo);
#endif
        if (pInode) {
            /*
             * Initialize the two structures.
             */
#ifdef VBOX_STRICT
            sf_new_i->u32Magic      = SF_INODE_INFO_MAGIC;
#endif
            sf_new_i->path          = path;
            sf_new_i->force_restat  = false;
            sf_new_i->ts_up_to_date = jiffies;
            RTListInit(&sf_new_i->HandleList);
            sf_new_i->handle        = SHFL_HANDLE_NIL;

            VBSF_SET_INODE_INFO(pInode, sf_new_i);
            vbsf_init_inode(pInode, sf_new_i, pObjInfo, sf_g);

            /*
             * Before we unlock the new inode, we may need to call d_instantiate.
             */
            if (fInstantiate)
                d_instantiate(dentry, pInode);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 25)
            unlock_new_inode(pInode);
#endif
            return pInode;

        }
        LogFunc(("iget failed\n"));
        kfree(sf_new_i);
    } else
        LogRelFunc(("could not allocate memory for new inode info\n"));
    return NULL;
}


/** Helper for vbsf_create_worker() and vbsf_inode_lookup() that wraps
 *  d_add() and setting d_op. */
DECLINLINE(void) vbsf_d_add_inode(struct dentry *dentry, struct inode *pNewInode)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
    Assert(dentry->d_op == &vbsf_dentry_ops); /* (taken from the superblock) */
#else
    dentry->d_op = &vbsf_dentry_ops;
#endif
    d_add(dentry, pNewInode);
}


/**
 * This is called when vfs failed to locate dentry in the cache. The
 * job of this function is to allocate inode and link it to dentry.
 * [dentry] contains the name to be looked in the [parent] directory.
 * Failure to locate the name is not a "hard" error, in this case NULL
 * inode is added to [dentry] and vfs should proceed trying to create
 * the entry via other means. NULL(or "positive" pointer) ought to be
 * returned in case of success and "negative" pointer on error
 */
static struct dentry *vbsf_inode_lookup(struct inode *parent, struct dentry *dentry
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
                                        , unsigned int flags
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
                                        , struct nameidata *nd
#endif
                                        )
{
    struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(parent->i_sb);
    struct vbsf_inode_info *sf_i = VBSF_GET_INODE_INFO(parent);
    SHFLSTRING             *path;
    struct dentry          *dret;
    int                     rc;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
    SFLOGFLOW(("vbsf_inode_lookup: parent=%p dentry=%p flags=%#x\n", parent, dentry, flags));
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    SFLOGFLOW(("vbsf_inode_lookup: parent=%p dentry=%p nd=%p{.flags=%#x}\n", parent, dentry, nd, nd ? nd->flags : 0));
#else
    SFLOGFLOW(("vbsf_inode_lookup: parent=%p dentry=%p\n", parent, dentry));
#endif

    Assert(sf_g);
    Assert(sf_i && sf_i->u32Magic == SF_INODE_INFO_MAGIC);

    /*
     * Build the path.  We'll associate the path with dret's inode on success.
     */
    rc = vbsf_path_from_dentry(__func__, sf_g, sf_i, dentry, &path);
    if (rc == 0) {
        /*
         * Do a lookup on the host side.
         */
        VBOXSFCREATEREQ *pReq = (VBOXSFCREATEREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq) + path->u16Size);
        if (pReq) {
            struct inode *pInode = NULL;

            RT_ZERO(*pReq);
            memcpy(&pReq->StrPath, path, SHFLSTRING_HEADER_SIZE + path->u16Size);
            pReq->CreateParms.Handle = SHFL_HANDLE_NIL;
            pReq->CreateParms.CreateFlags = SHFL_CF_LOOKUP | SHFL_CF_ACT_FAIL_IF_NEW;

            LogFunc(("Calling VbglR0SfHostReqCreate on %s\n", path->String.utf8));
            rc = VbglR0SfHostReqCreate(sf_g->map.root, pReq);
            if (RT_SUCCESS(rc)) {
                if (pReq->CreateParms.Result == SHFL_FILE_EXISTS) {
                    /*
                     * Create an inode for the result.  Since this also confirms
                     * the existence of all parent dentries, we increase their TTL.
                     */
                    pInode = vbsf_create_inode(parent, dentry, path, &pReq->CreateParms.Info, sf_g, false /*fInstantiate*/);
                    if (rc == 0) {
                        path = NULL; /* given to the inode */
                        dret = dentry;
                    } else
                        dret = (struct dentry *)ERR_PTR(-ENOMEM);
                    vbsf_dentry_chain_increase_parent_ttl(dentry);
                } else if (   pReq->CreateParms.Result == SHFL_FILE_NOT_FOUND
                       || pReq->CreateParms.Result == SHFL_PATH_NOT_FOUND /*this probably should happen*/) {
                    dret = dentry;
                } else {
                    AssertMsgFailed(("%d\n", pReq->CreateParms.Result));
                    dret = (struct dentry *)ERR_PTR(-EPROTO);
                }
            } else if (rc == VERR_INVALID_NAME) {
                dret = dentry; /* this can happen for names like 'foo*' on a Windows host */
            } else {
                LogFunc(("VbglR0SfHostReqCreate failed on %s: %Rrc\n", path->String.utf8, rc));
                dret = (struct dentry *)ERR_PTR(-EPROTO);
            }
            VbglR0PhysHeapFree(pReq);

            /*
             * When dret is set to dentry we got something to insert,
             * though it may be negative (pInode == NULL).
             */
            if (dret == dentry) {
                vbsf_dentry_set_update_jiffies(dentry, jiffies);
                vbsf_d_add_inode(dentry, pInode);
                dret = NULL;
            }
        } else
            dret = (struct dentry *)ERR_PTR(-ENOMEM);
        if (path)
            kfree(path);
    } else
        dret = (struct dentry *)ERR_PTR(rc);
    return dret;
}


/**
 * This should allocate memory for vbsf_inode_info, compute a unique inode
 * number, get an inode from vfs, initialize inode info, instantiate
 * dentry.
 *
 * @param parent        inode entry of the directory
 * @param dentry        directory cache entry
 * @param path          path name.  Consumed on success.
 * @param info          file information
 * @param handle        handle
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_inode_instantiate(struct inode *parent, struct dentry *dentry, PSHFLSTRING path,
                                  PSHFLFSOBJINFO info, SHFLHANDLE handle)
{
    struct vbsf_super_info *sf_g   = VBSF_GET_SUPER_INFO(parent->i_sb);
    struct inode           *pInode = vbsf_create_inode(parent, dentry, path, info, sf_g, true /*fInstantiate*/);
    if (pInode) {
        /* Store this handle if we leave the handle open. */
        struct vbsf_inode_info *sf_new_i = VBSF_GET_INODE_INFO(pInode);
        sf_new_i->handle = handle;
        return 0;
    }
    return -ENOMEM;
}


/**
 * Create a new regular file / directory.
 *
 * @param parent        inode of the directory
 * @param dentry        directory cache entry
 * @param mode          file mode
 * @param fCreateFlags  SHFL_CF_XXX.
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_create_worker(struct inode *parent, struct dentry *dentry, umode_t mode, uint32_t fCreateFlags,
                              bool fStashHandle, bool fDoLookup, SHFLHANDLE *phHostFile, bool *pfCreated)

{
#ifdef SFLOG_ENABLED
    const char * const      pszPrefix   = S_ISDIR(mode) ? "vbsf_create_worker/dir:" : "vbsf_create_worker/file:";
#endif
    struct vbsf_inode_info *sf_parent_i = VBSF_GET_INODE_INFO(parent);
    struct vbsf_super_info *sf_g        = VBSF_GET_SUPER_INFO(parent->i_sb);
    PSHFLSTRING             path;
    int                     rc;

    AssertReturn(sf_parent_i, -EINVAL);
    AssertReturn(sf_g, -EINVAL);

    /*
     * Build a path.  We'll donate this to the inode on success.
     */
    rc = vbsf_path_from_dentry(__func__, sf_g, sf_parent_i, dentry, &path);
    if (rc == 0) {
        /*
         * Allocate, initialize and issue the SHFL_CREATE request.
         */
        /** @todo combine with vbsf_path_from_dentry? */
        union CreateAuxReq
        {
            VBOXSFCREATEREQ Create;
            VBOXSFCLOSEREQ  Close;
        } *pReq = (union CreateAuxReq *)VbglR0PhysHeapAlloc(RT_UOFFSETOF(VBOXSFCREATEREQ, StrPath.String) + path->u16Size);
        if (pReq) {
            memcpy(&pReq->Create.StrPath, path, SHFLSTRING_HEADER_SIZE + path->u16Size);

            RT_ZERO(pReq->Create.CreateParms);
            pReq->Create.CreateParms.Handle                  = SHFL_HANDLE_NIL;
            pReq->Create.CreateParms.CreateFlags             = fCreateFlags;
            pReq->Create.CreateParms.Info.Attr.fMode         = (S_ISDIR(mode) ? RTFS_TYPE_DIRECTORY : RTFS_TYPE_FILE)
                                                             | sf_access_permissions_to_vbox(mode);
            pReq->Create.CreateParms.Info.Attr.enmAdditional = RTFSOBJATTRADD_NOTHING;

            SFLOGFLOW(("%s calling VbglR0SfHostReqCreate(%s, %#x)\n", pszPrefix, path->String.ach, pReq->Create.CreateParms.CreateFlags));
            rc = VbglR0SfHostReqCreate(sf_g->map.root, &pReq->Create);
            if (RT_SUCCESS(rc)) {
                SFLOGFLOW(("%s VbglR0SfHostReqCreate returned %Rrc Result=%d Handle=%#llx\n",
                           pszPrefix, rc, pReq->Create.CreateParms.Result, pReq->Create.CreateParms.Handle));

                /*
                 * Work the dentry cache and inode restatting.
                 */
                if (   pReq->Create.CreateParms.Result == SHFL_FILE_CREATED
                    || pReq->Create.CreateParms.Result == SHFL_FILE_REPLACED) {
                    vbsf_dentry_chain_increase_parent_ttl(dentry);
                    sf_parent_i->force_restat = 1;
                } else if (   pReq->Create.CreateParms.Result == SHFL_FILE_EXISTS
                           || pReq->Create.CreateParms.Result == SHFL_FILE_NOT_FOUND)
                    vbsf_dentry_chain_increase_parent_ttl(dentry);

                /*
                 * If we got a handle back, we're good.  Create an inode for it and return.
                 */
                if (pReq->Create.CreateParms.Handle != SHFL_HANDLE_NIL) {
                    struct inode *pNewInode = vbsf_create_inode(parent, dentry, path, &pReq->Create.CreateParms.Info, sf_g,
                                                                !fDoLookup /*fInstantiate*/);
                    if (pNewInode) {
                        struct vbsf_inode_info *sf_new_i = VBSF_GET_INODE_INFO(pNewInode);
                        if (phHostFile) {
                            *phHostFile = pReq->Create.CreateParms.Handle;
                            pReq->Create.CreateParms.Handle = SHFL_HANDLE_NIL;
                        } else if (fStashHandle) {
                            sf_new_i->handle = pReq->Create.CreateParms.Handle;
                            pReq->Create.CreateParms.Handle = SHFL_HANDLE_NIL;
                        }
                        if (fDoLookup)
                            vbsf_d_add_inode(dentry, pNewInode);
                        path = NULL;
                    } else {
                        SFLOGFLOW(("%s vbsf_create_inode failed: -ENOMEM (path %s)\n", pszPrefix, rc, path->String.ach));
                        rc = -ENOMEM;
                    }
                } else if (pReq->Create.CreateParms.Result == SHFL_FILE_EXISTS) {
                    /*
                     * For atomic_open (at least), we should create an inode and
                     * convert the dentry from a negative to a positive one.
                     */
                    SFLOGFLOW(("%s SHFL_FILE_EXISTS for %s\n", pszPrefix, sf_parent_i->path->String.ach));
                    if (fDoLookup) {
                        struct inode *pNewInode = vbsf_create_inode(parent, dentry, path, &pReq->Create.CreateParms.Info,
                                                                    sf_g, false /*fInstantiate*/);
                        if (pNewInode)
                            vbsf_d_add_inode(dentry, pNewInode);
                        path = NULL;
                    }
                    rc = -EEXIST;
                } else if (pReq->Create.CreateParms.Result == SHFL_FILE_NOT_FOUND) {
                    SFLOGFLOW(("%s SHFL_FILE_NOT_FOUND for %s\n", pszPrefix, sf_parent_i->path->String.ach));
                    rc = -ENOENT;
                } else if (pReq->Create.CreateParms.Result == SHFL_PATH_NOT_FOUND) {
                    SFLOGFLOW(("%s SHFL_PATH_NOT_FOUND for %s\n", pszPrefix, sf_parent_i->path->String.ach));
                    rc = -ENOENT;
                } else {
                    AssertMsgFailed(("result=%d creating '%s'\n", pReq->Create.CreateParms.Result, sf_parent_i->path->String.ach));
                    rc = -EPERM;
                }
            } else {
                int const vrc = rc;
                rc = -RTErrConvertToErrno(vrc);
                SFLOGFLOW(("%s SHFL_FN_CREATE(%s) failed vrc=%Rrc rc=%d\n", pszPrefix, path->String.ach, vrc, rc));
            }

            /* Cleanups. */
            if (pReq->Create.CreateParms.Handle != SHFL_HANDLE_NIL) {
                AssertCompile(RTASSERT_OFFSET_OF(VBOXSFCREATEREQ, CreateParms.Handle) > sizeof(VBOXSFCLOSEREQ)); /* no aliasing issues */
                int rc2 = VbglR0SfHostReqClose(sf_g->map.root, &pReq->Close, pReq->Create.CreateParms.Handle);
                if (RT_FAILURE(rc2))
                    SFLOGFLOW(("%s VbglR0SfHostReqCloseSimple failed rc=%Rrc\n", pszPrefix, rc2));
            }
            VbglR0PhysHeapFree(pReq);
        } else
            rc = -ENOMEM;
        if (path)
            kfree(path);
    }
    return rc;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
/**
 * More atomic way of handling creation.
 *
 * Older kernels would first to a lookup that created the file, followed by
 * an open call.  We've got this horrid vbsf_inode_info::handle member because
 * of that approach.  The call combines the lookup and open.
 */
static int vbsf_inode_atomic_open(struct inode *pDirInode, struct dentry *dentry, struct file *file,  unsigned fOpen,
                                  umode_t fMode
# if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
                                  , int *opened
# endif
                                  )
{
    SFLOGFLOW(("vbsf_inode_atomic_open: pDirInode=%p dentry=%p file=%p fOpen=%#x, fMode=%#x\n", pDirInode, dentry, file, fOpen, fMode));
    int rc;

    /* Code assumes negative dentry. */
    Assert(dentry->d_inode == NULL);

    /** @todo see if we can do this for non-create calls too, as it may save us a
     *        host call to revalidate the dentry. (Can't see anyone else doing
     *        this, so playing it safe for now.) */
    if (fOpen & O_CREAT) {
        /*
         * Prepare our file info structure.
         */
        struct vbsf_reg_info *sf_r = kmalloc(sizeof(*sf_r), GFP_KERNEL);
        if (sf_r) {
            bool     fCreated = false;
            uint32_t fCreateFlags;

            RTListInit(&sf_r->Handle.Entry);
            sf_r->Handle.cRefs  = 1;
            sf_r->Handle.fFlags = !(fOpen & O_DIRECTORY)
                                 ? VBSF_HANDLE_F_FILE | VBSF_HANDLE_F_MAGIC
                                 : VBSF_HANDLE_F_DIR  | VBSF_HANDLE_F_MAGIC;
            sf_r->Handle.hHost  = SHFL_HANDLE_NIL;

            /*
             * Try create it.
             */
            /* vbsf_create_worker uses the type from fMode, so match it up to O_DIRECTORY. */
            AssertMsg(!(fMode & S_IFMT) || (fMode & S_IFMT) == (fOpen & O_DIRECTORY ? S_IFDIR : S_IFREG), ("0%o\n", fMode));
            if (!(fOpen & O_DIRECTORY))
                fMode = (fMode & ~S_IFMT) | S_IFREG;
            else
                fMode = (fMode & ~S_IFMT) | S_IFDIR;

            fCreateFlags = vbsf_linux_oflags_to_vbox(fOpen, &sf_r->Handle.fFlags, __FUNCTION__);

            rc = vbsf_create_worker(pDirInode, dentry, fMode, fCreateFlags, false /*fStashHandle*/, true /*fDoLookup*/,
                                    &sf_r->Handle.hHost, &fCreated);
            if (rc == 0) {
                struct inode           *inode = dentry->d_inode;
                struct vbsf_inode_info *sf_i  = VBSF_GET_INODE_INFO(inode);

                /*
                 * Set FMODE_CREATED according to the action taken by SHFL_CREATE
                 * and call finish_open() to do the remaining open() work.
                 */
# if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
                if (fCreated)
                    file->f_mode |= FMODE_CREATED;
                rc = finish_open(file, dentry, generic_file_open);
# else
                if (fCreated)
                    *opened |= FILE_CREATED;
                rc = finish_open(file, dentry, generic_file_open, opened);
# endif
                if (rc == 0) {
                    /*
                     * Now that the file is fully opened, associate sf_r with it
                     * and link the handle to the inode.
                     */
                    vbsf_handle_append(sf_i, &sf_r->Handle);
                    file->private_data = sf_r;
                    SFLOGFLOW(("vbsf_inode_atomic_open: create succeeded; hHost=%#llx path='%s'\n",
                               rc, sf_r->Handle.hHost, sf_i->path->String.ach));
                    sf_r = NULL; /* don't free it */
                } else {
                    struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(pDirInode->i_sb);
                    SFLOGFLOW(("vbsf_inode_atomic_open: finish_open failed: %d (path='%s'\n", rc, sf_i->path->String.ach));
                    VbglR0SfHostReqCloseSimple(sf_g->map.root, sf_r->Handle.hHost);
                    sf_r->Handle.hHost = SHFL_HANDLE_NIL;
                }
            } else
                SFLOGFLOW(("vbsf_inode_atomic_open: vbsf_create_worker failed: %d\n", rc));
            if (sf_r)
                kfree(sf_r);
        } else {
            LogRelMaxFunc(64, ("could not allocate reg info\n"));
            rc = -ENOMEM;
        }
    }
    /*
     * Not creating anything.
     * Do we need to do a lookup or should we just fail?
     */
    else if (d_in_lookup(dentry)) {
        struct dentry *pResult = vbsf_inode_lookup(pDirInode, dentry, 0 /*fFlags*/);
        if (!IS_ERR(pResult))
            rc = finish_no_open(file, pResult);
        else
            rc = PTR_ERR(pResult);
        SFLOGFLOW(("vbsf_inode_atomic_open: open -> %d (%p)\n", rc, pResult));
    } else {
        SFLOGFLOW(("vbsf_inode_atomic_open: open -> -ENOENT\n"));
        rc = -ENOENT;
    }
    return rc;
}
#endif /* 3.6.0 */


/**
 * Create a new regular file.
 *
 * @param parent        inode of the directory
 * @param dentry        directory cache entry
 * @param mode          file mode
 * @param excl          Possible O_EXCL...
 * @returns 0 on success, Linux error code otherwise
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0) || defined(DOXYGEN_RUNNING)
static int vbsf_inode_create(struct inode *parent, struct dentry *dentry, umode_t mode, bool excl)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
static int vbsf_inode_create(struct inode *parent, struct dentry *dentry, umode_t mode, struct nameidata *nd)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 75)
static int vbsf_inode_create(struct inode *parent, struct dentry *dentry, int mode, struct nameidata *nd)
#else
static int vbsf_inode_create(struct inode *parent, struct dentry *dentry, int mode)
#endif
{
    uint32_t fCreateFlags = SHFL_CF_ACT_CREATE_IF_NEW
                          | SHFL_CF_ACT_FAIL_IF_EXISTS
                          | SHFL_CF_ACCESS_READWRITE;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0) && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 75)
    /* Clear the RD flag if write-only access requested.  Otherwise assume we
       need write access to create stuff. */
    if (!(nd->intent.open.flags & 1) ) {
        fCreateFlags &= SHFL_CF_ACCESS_READWRITE;
        fCreateFlags |= SHFL_CF_ACCESS_WRITE;
    }
    /* (file since 2.6.15) */
#endif
    TRACE();
    AssertMsg(!(mode & S_IFMT) || (mode & S_IFMT) == S_IFREG, ("0%o\n", mode));
    return vbsf_create_worker(parent, dentry, (mode & ~S_IFMT) | S_IFREG, fCreateFlags,
                              true /*fStashHandle*/, false /*fDoLookup*/, NULL /*phHandle*/, NULL /*fCreated*/);
}

/**
 * Create a new directory.
 *
 * @param parent        inode of the directory
 * @param dentry        directory cache entry
 * @param mode          file mode
 * @returns 0 on success, Linux error code otherwise
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
static int vbsf_inode_mkdir(struct inode *parent, struct dentry *dentry, umode_t mode)
#else
static int vbsf_inode_mkdir(struct inode *parent, struct dentry *dentry, int mode)
#endif
{
    TRACE();
    AssertMsg(!(mode & S_IFMT) || (mode & S_IFMT) == S_IFDIR, ("0%o\n", mode));
    return vbsf_create_worker(parent, dentry, (mode & ~S_IFMT) | S_IFDIR,
                                SHFL_CF_ACT_CREATE_IF_NEW
                              | SHFL_CF_ACT_FAIL_IF_EXISTS
                              | SHFL_CF_ACCESS_READWRITE
                              | SHFL_CF_DIRECTORY,
                              false /*fStashHandle*/, false /*fDoLookup*/, NULL /*phHandle*/, NULL /*fCreated*/);
}

/**
 * Remove a regular file / directory.
 *
 * @param parent        inode of the directory
 * @param dentry        directory cache entry
 * @param fDirectory    true if directory, false otherwise
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_unlink_worker(struct inode *parent, struct dentry *dentry, int fDirectory)
{
    int rc, err;
    struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(parent->i_sb);
    struct vbsf_inode_info *sf_parent_i = VBSF_GET_INODE_INFO(parent);
    SHFLSTRING *path;

    TRACE();
    BUG_ON(!sf_g);

    err = vbsf_path_from_dentry(__func__, sf_g, sf_parent_i, dentry, &path);
    if (!err) {
        VBOXSFREMOVEREQ *pReq = (VBOXSFREMOVEREQ *)VbglR0PhysHeapAlloc(RT_UOFFSETOF(VBOXSFREMOVEREQ, StrPath.String)
                                           + path->u16Size);
        if (pReq) {
            memcpy(&pReq->StrPath, path, SHFLSTRING_HEADER_SIZE + path->u16Size);
            uint32_t fFlags = fDirectory ? SHFL_REMOVE_DIR : SHFL_REMOVE_FILE;
            if (dentry->d_inode && ((dentry->d_inode->i_mode & S_IFLNK) == S_IFLNK))
                fFlags |= SHFL_REMOVE_SYMLINK;

            rc = VbglR0SfHostReqRemove(sf_g->map.root, pReq, fFlags);

            if (dentry->d_inode) {
                struct vbsf_inode_info *sf_i = VBSF_GET_INODE_INFO(dentry->d_inode);
                sf_i->force_restat = true;
            }

            if (RT_SUCCESS(rc)) {
                sf_parent_i->force_restat = true; /* directory access/change time changed */
                err = 0;
            } else if (rc == VERR_FILE_NOT_FOUND || rc == VERR_PATH_NOT_FOUND) {
                LogFunc(("(%d): VbglR0SfRemove(%s) failed rc=%Rrc; calling d_drop on %p\n",
                         fDirectory, path->String.ach, rc, dentry));
                d_drop(dentry);
            } else {
                LogFunc(("(%d): VbglR0SfRemove(%s) failed rc=%Rrc\n", fDirectory, path->String.ach, rc));
                err = -RTErrConvertToErrno(rc);
            }
            VbglR0PhysHeapFree(pReq);
        } else
            err = -ENOMEM;
        kfree(path);
    }
    return err;
}

/**
 * Remove a regular file.
 *
 * @param parent        inode of the directory
 * @param dentry        directory cache entry
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_inode_unlink(struct inode *parent, struct dentry *dentry)
{
    TRACE();
    return vbsf_unlink_worker(parent, dentry, false /*fDirectory*/);
}

/**
 * Remove a directory.
 *
 * @param parent        inode of the directory
 * @param dentry        directory cache entry
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_inode_rmdir(struct inode *parent, struct dentry *dentry)
{
    TRACE();
    return vbsf_unlink_worker(parent, dentry, true /*fDirectory*/);
}

/**
 * Rename a regular file / directory.
 *
 * @param old_parent    inode of the old parent directory
 * @param old_dentry    old directory cache entry
 * @param new_parent    inode of the new parent directory
 * @param new_dentry    new directory cache entry
 * @param flags         flags
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_inode_rename(struct inode *old_parent, struct dentry *old_dentry,
                             struct inode *new_parent, struct dentry *new_dentry
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
                             , unsigned flags
#endif
                             )
{
    int err = 0, rc = VINF_SUCCESS;
    struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(old_parent->i_sb);

    TRACE();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    if (flags) {
        LogFunc(("rename with flags=%x\n", flags));
        return -EINVAL;
    }
#endif

    if (sf_g != VBSF_GET_SUPER_INFO(new_parent->i_sb)) {
        LogFunc(("rename with different roots\n"));
        err = -EINVAL;
    } else {
        struct vbsf_inode_info *sf_old_i = VBSF_GET_INODE_INFO(old_parent);
        struct vbsf_inode_info *sf_new_i = VBSF_GET_INODE_INFO(new_parent);
        /* As we save the relative path inside the inode structure, we need to change
           this if the rename is successful. */
        struct vbsf_inode_info *sf_file_i = VBSF_GET_INODE_INFO(old_dentry->d_inode);
        SHFLSTRING *old_path;
        SHFLSTRING *new_path;

        BUG_ON(!sf_old_i);
        BUG_ON(!sf_new_i);
        BUG_ON(!sf_file_i);

        old_path = sf_file_i->path;
        err = vbsf_path_from_dentry(__func__, sf_g, sf_new_i, new_dentry, &new_path);
        if (err)
            LogFunc(("failed to create new path\n"));
        else {
            VBOXSFRENAMEWITHSRCBUFREQ *pReq;
            pReq = (VBOXSFRENAMEWITHSRCBUFREQ *)VbglR0PhysHeapAlloc(RT_UOFFSETOF(VBOXSFRENAMEWITHSRCBUFREQ, StrDstPath.String)
                                                                    + new_path->u16Size);
            if (pReq) {
                memcpy(&pReq->StrDstPath, new_path, SHFLSTRING_HEADER_SIZE + new_path->u16Size);
                rc = VbglR0SfHostReqRenameWithSrcContig(sf_g->map.root, pReq,
                                                        old_path, virt_to_phys(old_path),
                                                        old_dentry->d_inode->i_mode & S_IFDIR ? SHFL_RENAME_DIR
                                                        : SHFL_RENAME_FILE | SHFL_RENAME_REPLACE_IF_EXISTS);
                VbglR0PhysHeapFree(pReq);
            } else
                rc = VERR_NO_MEMORY;
            if (RT_SUCCESS(rc)) {
                kfree(old_path);
                sf_new_i->force_restat = 1;
                sf_old_i->force_restat = 1; /* XXX: needed? */

                /* Set the new relative path in the inode. */
                sf_file_i->path = new_path;
            } else {
                LogFunc(("VbglR0SfRename failed rc=%Rrc\n",
                     rc));
                err = -RTErrConvertToErrno(rc);
                kfree(new_path);
            }
        }
    }
    return err;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
static int vbsf_ino_symlink(struct inode *parent, struct dentry *dentry, const char *symname)
{
    int err;
    int rc;
    struct vbsf_inode_info *sf_i;
    struct vbsf_super_info *sf_g;
    SHFLSTRING *path, *ssymname;
    SHFLFSOBJINFO info;
    int symname_len = strlen(symname) + 1;

    TRACE();
    sf_g = VBSF_GET_SUPER_INFO(parent->i_sb);
    sf_i = VBSF_GET_INODE_INFO(parent);

    BUG_ON(!sf_g);
    BUG_ON(!sf_i);

    err = vbsf_path_from_dentry(__func__, sf_g, sf_i, dentry, &path);
    if (err)
        goto fail0;

    ssymname = kmalloc(offsetof(SHFLSTRING, String.utf8) + symname_len, GFP_KERNEL);
    if (!ssymname) {
        LogRelFunc(("kmalloc failed, caller=sf_symlink\n"));
        err = -ENOMEM;
        goto fail1;
    }

    ssymname->u16Length = symname_len - 1;
    ssymname->u16Size = symname_len;
    memcpy(ssymname->String.utf8, symname, symname_len);

    rc = VbglR0SfSymlink(&g_SfClient, &sf_g->map, path, ssymname, &info);
    kfree(ssymname);

    if (RT_FAILURE(rc)) {
        err = RTErrConvertFromErrno(rc);
        LogFunc(("VbglR0SfSymlink(%s) failed rc=%Rrc\n", sf_i->path->String.utf8, rc));
        goto fail1;
    }

    err = vbsf_inode_instantiate(parent, dentry, path, &info, SHFL_HANDLE_NIL);
    if (err) {
        LogFunc(("could not instantiate dentry for %s err=%d\n", sf_i->path->String.utf8, err));
        goto fail1;
    }

    sf_i->force_restat = 1;
    return 0;

 fail1:
    kfree(path);
 fail0:
    return err;
}
#endif /* LINUX_VERSION_CODE >= 2.6.0 */

struct inode_operations vbsf_dir_iops = {
    .lookup = vbsf_inode_lookup,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
    .atomic_open = vbsf_inode_atomic_open,
#endif
    .create = vbsf_inode_create,
    .mkdir = vbsf_inode_mkdir,
    .rmdir = vbsf_inode_rmdir,
    .unlink = vbsf_inode_unlink,
    .rename = vbsf_inode_rename,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 18)
    .getattr = vbsf_inode_getattr,
#else
    .revalidate = vbsf_inode_revalidate,
#endif
    .setattr = vbsf_inode_setattr,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    .symlink = vbsf_ino_symlink,
#endif
};

