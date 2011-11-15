// Definitions for SCSI style command data blocks.
#ifndef __BLOCKCMD_H
#define __BLOCKCMD_H

#include "types.h" // u8

#define CDB_CMD_READ_10 0x28
#define CDB_CMD_VERIFY_10 0x2f
#define CDB_CMD_WRITE_10 0x2a

struct cdb_rwdata_10 {
    u8 command;
    u8 flags;
    u32 lba;
    u8 resreved_06;
    u16 count;
    u8 reserved_09;
    u8 pad[6];
} PACKED;

#define CDB_CMD_READ_CAPACITY 0x25

struct cdb_read_capacity {
    u8 command;
    u8 flags;
    u8 resreved_02[8];
    u8 pad[6];
} PACKED;

struct cdbres_read_capacity {
    u32 sectors;
    u32 blksize;
} PACKED;

#define CDB_CMD_TEST_UNIT_READY  0x00
#define CDB_CMD_INQUIRY          0x12
#define CDB_CMD_REQUEST_SENSE    0x03

struct cdb_request_sense {
    u8 command;
    u8 flags;
    u16 reserved_02;
    u8 length;
    u8 reserved_05;
    u8 pad[10];
} PACKED;

struct cdbres_request_sense {
    u8 errcode;
    u8 segment;
    u8 flags;
    u32 info;
    u8 additional;
    u32 specific;
    u8 asc;
    u8 ascq;
    u32 reserved_0e;
} PACKED;

#define SCSI_TYPE_DISK  0x00
#define SCSI_TYPE_CDROM 0x05

struct cdbres_inquiry {
    u8 pdt;
    u8 removable;
    u8 reserved_02[2];
    u8 additional;
    u8 reserved_05[3];
    char vendor[8];
    char product[16];
    char rev[4];
} PACKED;

#define CDB_CMD_MODE_SENSE    0x5A
#define MODE_PAGE_HD_GEOMETRY 0x04

struct cdb_mode_sense {
    u8 command;
    u8 flags;
    u8 page;
    u32 reserved_03;
    u16 count;
    u8 reserved_09;
    u8 pad[6];
} PACKED;

struct cdbres_mode_sense_geom {
    u8 unused_00[3];
    u8 read_only;
    u32 unused_04;
    u8 page;
    u8 length;
    u8 cyl[3];
    u8 heads;
    u8 precomp[3];
    u8 reduced[3];
    u16 step_rate;
    u8 landing[3];
    u16 rpm;
} PACKED;

// blockcmd.c
int cdb_get_inquiry(struct disk_op_s *op, struct cdbres_inquiry *data);
int cdb_get_sense(struct disk_op_s *op, struct cdbres_request_sense *data);
int cdb_test_unit_ready(struct disk_op_s *op);
int cdb_read_capacity(struct disk_op_s *op, struct cdbres_read_capacity *data);
int cdb_mode_sense_geom(struct disk_op_s *op, struct cdbres_mode_sense_geom *data);
int cdb_inquiry(struct disk_op_s *op, struct cdbres_inquiry *data);
int cdb_read(struct disk_op_s *op);
int cdb_write(struct disk_op_s *op);

int scsi_is_ready(struct disk_op_s *op);
int scsi_init_drive(struct drive_s *drive, const char *s, int *pdt, char **desc);

#endif // blockcmd.h
