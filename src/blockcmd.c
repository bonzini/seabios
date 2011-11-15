// Support for several common scsi like command data block requests
//
// Copyright (C) 2010  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2002  MandrakeSoft S.A.
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "biosvar.h" // GET_GLOBAL
#include "util.h" // htonl
#include "disk.h" // struct disk_op_s
#include "blockcmd.h" // struct cdb_request_sense
#include "ata.h" // atapi_cmd_data
#include "ahci.h" // atapi_cmd_data
#include "usb-msc.h" // usb_cmd_data

// Route command to low-level handler.
static int
cdb_cmd_data(struct disk_op_s *op, void *cdbcmd, u16 blocksize)
{
    u8 type = GET_GLOBAL(op->drive_g->type);
    switch (type) {
    case DTYPE_ATAPI:
        return atapi_cmd_data(op, cdbcmd, blocksize);
    case DTYPE_USB:
        return usb_cmd_data(op, cdbcmd, blocksize);
    case DTYPE_AHCI:
        return ahci_cmd_data(op, cdbcmd, blocksize);
    default:
        op->count = 0;
        return DISK_RET_EPARAM;
    }
}

int
scsi_is_ready(struct disk_op_s *op)
{
    dprintf(6, "scsi_is_ready (drive=%p)\n", op->drive_g);

    /* Retry TEST UNIT READY for 5 seconds unless MEDIUM NOT PRESENT is
     * reported by the device.  If the device reports "IN PROGRESS",
     * 30 seconds is added. */
    int in_progress = 0;
    u64 end = calc_future_tsc(5000);
    for (;;) {
        if (check_tsc(end)) {
            dprintf(1, "test unit ready failed\n");
            return -1;
        }

        int ret = cdb_test_unit_ready(op);
        if (!ret)
            // Success
            break;

        struct cdbres_request_sense sense;
        ret = cdb_get_sense(op, &sense);
        if (ret)
            // Error - retry.
            continue;

        // Sense succeeded.
        if (sense.asc == 0x3a) { /* MEDIUM NOT PRESENT */
            dprintf(1, "Device reports MEDIUM NOT PRESENT\n");
            return -1;
        }

        if (sense.asc == 0x04 && sense.ascq == 0x01 && !in_progress) {
            /* IN PROGRESS OF BECOMING READY */
            printf("Waiting for device to detect medium... ");
            /* Allow 30 seconds more */
            end = calc_future_tsc(30000);
            in_progress = 1;
        }
    }
    return 0;
}

// Validate drive and find block size and sector count.
int
scsi_init_drive(struct drive_s *drive, const char *s, int *pdt, char **desc)
{
    struct disk_op_s dop;
    memset(&dop, 0, sizeof(dop));
    dop.drive_g = drive;
    struct cdbres_inquiry data;
    int ret = cdb_get_inquiry(&dop, &data);
    if (ret)
        return ret;
    char vendor[sizeof(data.vendor)+1], product[sizeof(data.product)+1];
    char rev[sizeof(data.rev)+1];
    strtcpy(vendor, data.vendor, sizeof(vendor));
    nullTrailingSpace(vendor);
    strtcpy(product, data.product, sizeof(product));
    nullTrailingSpace(product);
    strtcpy(rev, data.rev, sizeof(rev));
    nullTrailingSpace(rev);
    *pdt = data.pdt & 0x1f;
    int removable = !!(data.removable & 0x80);
    dprintf(1, "%s vendor='%s' product='%s' rev='%s' type=%d removable=%d\n"
            , s, vendor, product, rev, *pdt, removable);
    drive->removable = removable;

    if (scsi_is_ready(&dop))
        return ret;

    struct cdbres_read_capacity capdata;
    ret = cdb_read_capacity(&dop, &capdata);
    if (ret)
        return ret;

    // READ CAPACITY returns the address of the last block
    drive->blksize = ntohl(capdata.blksize);
    drive->sectors = ntohl(capdata.sectors) + 1;
    dprintf(1, "%s blksize=%d sectors=%d\n"
            , s, drive->blksize, (unsigned)drive->sectors);

    if (drive->blksize == CDROM_SECTOR_SIZE)
        *pdt = SCSI_TYPE_CDROM;

    if (*pdt == SCSI_TYPE_CDROM)
        *desc = znprintf(MAXDESCSIZE, "DVD/CD [%s Drive %s %s %s]"
                              , s, vendor, product, rev);
    else
        *desc = znprintf(MAXDESCSIZE, "%s Drive %s %s %s"
                              , s, vendor, product, rev);

    return 0;
}

int
cdb_get_inquiry(struct disk_op_s *op, struct cdbres_inquiry *data)
{
    struct cdb_request_sense cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command = CDB_CMD_INQUIRY;
    cmd.length = sizeof(*data);
    op->count = 1;
    op->buf_fl = data;
    return cdb_cmd_data(op, &cmd, sizeof(*data));
}

// Request SENSE
int
cdb_get_sense(struct disk_op_s *op, struct cdbres_request_sense *data)
{
    struct cdb_request_sense cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command = CDB_CMD_REQUEST_SENSE;
    cmd.length = sizeof(*data);
    op->count = 1;
    op->buf_fl = data;
    return cdb_cmd_data(op, &cmd, sizeof(*data));
}

// Test unit ready
int
cdb_test_unit_ready(struct disk_op_s *op)
{
    struct cdb_request_sense cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command = CDB_CMD_TEST_UNIT_READY;
    op->count = 0;
    op->buf_fl = NULL;
    return cdb_cmd_data(op, &cmd, 0);
}

// Request capacity
int
cdb_read_capacity(struct disk_op_s *op, struct cdbres_read_capacity *data)
{
    struct cdb_read_capacity cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command = CDB_CMD_READ_CAPACITY;
    op->count = 1;
    op->buf_fl = data;
    return cdb_cmd_data(op, &cmd, sizeof(*data));
}

// Read sectors.
int
cdb_read(struct disk_op_s *op)
{
    struct cdb_rwdata_10 cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command = CDB_CMD_READ_10;
    cmd.lba = htonl(op->lba);
    cmd.count = htons(op->count);
    return cdb_cmd_data(op, &cmd, GET_GLOBAL(op->drive_g->blksize));
}

// Write sectors.
int
cdb_write(struct disk_op_s *op)
{
    struct cdb_rwdata_10 cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command = CDB_CMD_WRITE_10;
    cmd.lba = htonl(op->lba);
    cmd.count = htons(op->count);
    return cdb_cmd_data(op, &cmd, GET_GLOBAL(op->drive_g->blksize));
}
