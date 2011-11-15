// Virtio SCSI boot support.
//
// Copyright (C) 2011 Red Hat Inc.
//
// Authors:
//  Paolo Bonzini <pbonzini@redhat.com>
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "util.h" // dprintf
#include "pci.h" // foreachpci
#include "config.h" // CONFIG_*
#include "biosvar.h" // GET_GLOBAL
#include "pci_ids.h" // PCI_DEVICE_ID_VIRTIO_BLK
#include "pci_regs.h" // PCI_VENDOR_ID
#include "boot.h" // boot_add_hd
#include "virtio-pci.h"
#include "virtio-ring.h"
#include "virtio-scsi.h"
#include "disk.h"

struct virtio_lun_s {
    struct drive_s drive;
    struct pci_device *pci;
    struct vring_virtqueue *vq;
    u16 ioaddr;
    u16 target;
    u16 lun;
};

static int
virtio_scsi_cmd(u16 ioaddr, struct vring_virtqueue *vq, struct disk_op_s *op,
                void *cdbcmd, u16 target, u16 lun, u32 len)
{
    struct virtio_scsi_req_cmd req;
    struct virtio_scsi_resp_cmd resp;
    struct vring_list sg[3];

    memset(&req, 0, sizeof(req));
    req.lun[0] = 1;
    req.lun[1] = target;
    req.lun[2] = (lun >> 8) | 0x40;
    req.lun[3] = (lun & 0xff);
    memcpy(req.cdb, cdbcmd, 16);

    int datain = (req.cdb[0] != CDB_CMD_WRITE_10);
    int data_idx = (datain ? 2 : 1);
    int out_num = (datain ? 1 : 2);
    int in_num = (op->count ? 3 : 2) - out_num;

    sg[0].addr   = MAKE_FLATPTR(GET_SEG(SS), &req);
    sg[0].length = sizeof(req);

    sg[out_num].addr   = MAKE_FLATPTR(GET_SEG(SS), &resp);
    sg[out_num].length = sizeof(resp);

    sg[data_idx].addr   = op->buf_fl;
    sg[data_idx].length = len;

    /* Add to virtqueue and kick host */
    vring_add_buf(vq, sg, out_num, in_num, 0, 0);
    vring_kick(ioaddr, vq, 1);

    /* Wait for reply */
    while (!vring_more_used(vq))
        usleep(5);

    /* Reclaim virtqueue element */
    vring_get_buf(vq, NULL);

    /* Clear interrupt status register.  Avoid leaving interrupts stuck if
     * VRING_AVAIL_F_NO_INTERRUPT was ignored and interrupts were raised.
     */
    vp_get_isr(ioaddr);

    if (resp.response == VIRTIO_BLK_S_OK && resp.status == 0) {
        return DISK_RET_SUCCESS;
    }
    return DISK_RET_EBADTRACK;
}

int
virtio_scsi_cmd_data(struct disk_op_s *op, void *cdbcmd, u16 blocksize)
{
    struct virtio_lun_s *vlun =
        container_of(op->drive_g, struct virtio_lun_s, drive);

    return virtio_scsi_cmd(GET_GLOBAL(vlun->ioaddr),
                           GET_GLOBAL(vlun->vq), op, cdbcmd,
                           GET_GLOBAL(vlun->target), GET_GLOBAL(vlun->lun),
                           blocksize * op->count);
}

static int
setup_lun_cdrom(struct virtio_lun_s *vlun, char *desc)
{
    int prio = bootprio_find_scsi_device(vlun->pci, vlun->target, vlun->lun);
    boot_add_cd(&vlun->drive, desc, prio);
    return 0;
}

static int
setup_lun_hd(struct virtio_lun_s *vlun, char *desc)
{
    if (vlun->drive.blksize != DISK_SECTOR_SIZE) {
        dprintf(1, "Unsupported block size %d\n", vlun->drive.blksize);
        return -1;
    }

    // Register with bcv system.
    int prio = bootprio_find_scsi_device(vlun->pci, vlun->target, vlun->lun);
    boot_add_hd(&vlun->drive, desc, prio);

    return 0;
}

static int
virtio_scsi_add_lun(struct pci_device *pci, u16 ioaddr,
                    struct vring_virtqueue *vq, u16 target, u16 lun)
{
    struct virtio_lun_s *vlun = malloc_fseg(sizeof(*vlun));
    if (!vlun) {
        warn_noalloc();
        return -1;
    }
    memset(vlun, 0, sizeof(*vlun));
    vlun->drive.type = DTYPE_VIRTIO_SCSI;
    vlun->drive.cntl_id = pci->bdf;
    vlun->pci = pci;
    vlun->ioaddr = ioaddr;
    vlun->vq = vq;
    vlun->target = target;
    vlun->lun = lun;

    int pdt, ret;
    char *desc = NULL;
    ret = scsi_init_drive(&vlun->drive, "virtio-scsi", &pdt, &desc);
    if (ret)
        goto fail;

    if (pdt == SCSI_TYPE_CDROM)
        ret = setup_lun_cdrom(vlun, desc);
    else
        ret = setup_lun_hd(vlun, desc);
    if (ret)
        goto fail;
    return ret;

fail:
    free(vlun);
    return -1;
}

static int
virtio_scsi_scan_target(struct pci_device *pci, u16 ioaddr,
                        struct vring_virtqueue *vq, u16 target)
{
    /* TODO: send REPORT LUNS.  For now, only LUN 0 is recognized.  */
    int ret = virtio_scsi_add_lun(pci, ioaddr, vq, target, 0);
    return ret < 0 ? ret : 1;
}

static void
init_virtio_scsi(struct pci_device *pci)
{
    u16 bdf = pci->bdf;
    dprintf(1, "found virtio-scsi at %x:%x\n", pci_bdf_to_bus(bdf),
            pci_bdf_to_dev(bdf));
    struct vring_virtqueue *vq = NULL;
    u16 ioaddr = vp_init_simple(bdf);
    if (vp_find_vq(ioaddr, 2, &vq) < 0 ) {
        if (vq) {
            dprintf(1, "fail to find vq for virtio-scsi %x:%x\n",
                    pci_bdf_to_bus(bdf), pci_bdf_to_dev(bdf));
        }
        goto fail;
    }

    struct virtio_scsi_config cfg;
    vp_get(ioaddr, 0, &cfg, sizeof(cfg));
    cfg.cdb_size   = VIRTIO_SCSI_CDB_SIZE;
    cfg.sense_size = VIRTIO_SCSI_SENSE_SIZE;
    vp_set(ioaddr, 0, &cfg, sizeof(cfg));

    int i, tot;
    for (tot = 0, i = 0; i < 256; i++)
        tot += virtio_scsi_scan_target(pci, ioaddr, vq, i);

    if (!tot)
        goto fail;

    vp_set_status(ioaddr, VIRTIO_CONFIG_S_ACKNOWLEDGE |
                  VIRTIO_CONFIG_S_DRIVER | VIRTIO_CONFIG_S_DRIVER_OK);
    return;

fail:
    free(vq);
}

void
virtio_scsi_setup(void)
{
    ASSERT32FLAT();
    if (! CONFIG_VIRTIO_SCSI || CONFIG_COREBOOT)
        return;

    dprintf(3, "init virtio-scsi\n");

    struct pci_device *pci;
    foreachpci(pci) {
        if (pci->vendor != PCI_VENDOR_ID_REDHAT_QUMRANET
            || pci->device != PCI_DEVICE_ID_VIRTIO_SCSI)
            continue;
        init_virtio_scsi(pci);
    }
}
