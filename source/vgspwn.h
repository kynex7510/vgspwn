#ifndef _VGSPWN_H
#define _VGSPWN_H

#include <3ds.h>

void vgspwnInit(void);
void vgspwnExit(void);

void vgspwnSelect(u8 target);
void vgspwnAddTransfer(u32 src, u32 dst, size_t size);
void vgspwnCommit(void);

#endif /* _VGSPWN_H */