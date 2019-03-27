/* Bit operations.
 *
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Rewritten based on bitops.c of redis for bitgo*/

#include "solidbits.h"

/* SETBIT key offset bitvalue */
/* SET 0 beyond the boundary will do nothing*/
int setbitCommand(struct setbit_cmd *cmd, struct desc_table *dt)
{
    size_t offset, bit;
    int byteval, bitval;
    uint8_t dst;

    /* Get current values */
    offset = cmd->offset >> 3;
    bit = 7 - (cmd->offset & 0x7);
    if (offset < dt->length)
    {
        if (!read_from(dt, (void *)&dst, 1, offset))
        {
            DRETURN(-1, 1);
        }
        byteval = dst;
        bitval = byteval & (1 << bit);
    }
    else
    {
        bitval = 0;
        byteval = 0;
        if (cmd->value == 0)
        {
            DRETURN(0, 1);
        }
    }
    /* Update byte with new bit value and return original value */
    byteval &= ~(1 << bit);
    byteval |= ((cmd->value & 0x1) << bit);
    dst = byteval;
    if (!write_to(dt, (void *)&dst, 1, offset))
    {
        DRETURN(-1, 1);
    }
    DRETURN(bitval ? 1 : 0, 1);
}

int getbitCommand(struct getbit_cmd *cmd, struct desc_table *dt)
{
    size_t offset, bit;
    int byteval, bitval;
    uint8_t dst;

    offset = cmd->offset >> 3;
    bit = 7 - (cmd->offset & 0x7);
    if (offset < dt->length)
    {
        if (!read_from(dt, (void *)&dst, 1, offset))
        {
            DRETURN(-1, 1);
        }
        byteval = dst;
        bitval = byteval & (1 << bit);
    }
    else
    {
        DRETURN(0, 1);
    }
    DRETURN(bitval ? 1 : 0, 1);
}

size_t redisPopcount(void *s, long count)
{
    size_t bits = 0;
    unsigned char *p;
    uint32_t *p4;
    static const unsigned char bitsinbyte[256] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};

    /* Count bits 28 bytes at a time */
    p4 = (uint32_t *)s;
    while (count >= 28)
    {
        uint32_t aux1, aux2, aux3, aux4, aux5, aux6, aux7;

        aux1 = *p4++;
        aux2 = *p4++;
        aux3 = *p4++;
        aux4 = *p4++;
        aux5 = *p4++;
        aux6 = *p4++;
        aux7 = *p4++;
        count -= 28;

        aux1 = aux1 - ((aux1 >> 1) & 0x55555555);
        aux1 = (aux1 & 0x33333333) + ((aux1 >> 2) & 0x33333333);
        aux2 = aux2 - ((aux2 >> 1) & 0x55555555);
        aux2 = (aux2 & 0x33333333) + ((aux2 >> 2) & 0x33333333);
        aux3 = aux3 - ((aux3 >> 1) & 0x55555555);
        aux3 = (aux3 & 0x33333333) + ((aux3 >> 2) & 0x33333333);
        aux4 = aux4 - ((aux4 >> 1) & 0x55555555);
        aux4 = (aux4 & 0x33333333) + ((aux4 >> 2) & 0x33333333);
        aux5 = aux5 - ((aux5 >> 1) & 0x55555555);
        aux5 = (aux5 & 0x33333333) + ((aux5 >> 2) & 0x33333333);
        aux6 = aux6 - ((aux6 >> 1) & 0x55555555);
        aux6 = (aux6 & 0x33333333) + ((aux6 >> 2) & 0x33333333);
        aux7 = aux7 - ((aux7 >> 1) & 0x55555555);
        aux7 = (aux7 & 0x33333333) + ((aux7 >> 2) & 0x33333333);
        bits += ((((aux1 + (aux1 >> 4)) & 0x0F0F0F0F) +
                  ((aux2 + (aux2 >> 4)) & 0x0F0F0F0F) +
                  ((aux3 + (aux3 >> 4)) & 0x0F0F0F0F) +
                  ((aux4 + (aux4 >> 4)) & 0x0F0F0F0F) +
                  ((aux5 + (aux5 >> 4)) & 0x0F0F0F0F) +
                  ((aux6 + (aux6 >> 4)) & 0x0F0F0F0F) +
                  ((aux7 + (aux7 >> 4)) & 0x0F0F0F0F)) *
                 0x01010101) >>
                24;
    }
    /* Count the remaining bytes. */
    p = (unsigned char *)p4;
    while (count--)
        bits += bitsinbyte[*p++];
    return bits;
}

long bitcountCommand(struct bitcount_cmd *cmd, struct desc_table *dt)
{
    size_t o = 0, r, t = 0, bits = 0;
    void *s;
    long count;

    size_t strlen = dt->length;
    if (cmd->start < 0 && cmd->end < 0 && cmd->start > cmd->end)
    {
        DRETURN(0, 1);
    }
    if (cmd->start < 0)
        cmd->start = strlen + cmd->start % (0 - strlen);
    if (cmd->end < 0)
        cmd->end = strlen - 1 + cmd->end % (0 - strlen);

    if (cmd->end >= strlen)
        cmd->end = strlen - 1;

    if (cmd->start > cmd->end)
    {
        DRETURN(0, 1);
    }
    count = cmd->end - cmd->start + 1;
    t = count / FILE_BUFFER_SIZE;
    if (count % FILE_BUFFER_SIZE != 0)
        t++;
    s = memalign(4, FILE_BUFFER_SIZE);
    o = cmd->start;
    while (t--)
    {
        r = read_from(dt, s, FILE_BUFFER_SIZE, o);
        o += r;
        bits += redisPopcount(s, r);
    }
    free(s);
    DRETURN(bits, 1);
}

void op_malloc(unsigned char **p, long count)
{
    while (count--)
    {
        p[count] = memalign(8, FILE_BUFFER_SIZE);
    }
}

void op_free(unsigned char **p, long count)
{
    while (count--)
    {
        free(p[count]);
    }
}

long bitopCommand(struct bitop_cmd *cmd, struct desc_table **dts)
{
    int i, maxidx, idx, times, size, offset = 0;
    size_t maxlen = 0, bits = 0;
    int t[cmd->count], r[cmd->count]; //always no use 0
    unsigned char *p[cmd->count];
    unsigned long *lp[cmd->count];
    for (i = 0; i < cmd->count; i++)
    {
        if (dts[i] != NULL)
        {
            t[i] = dts[i]->length / FILE_BUFFER_SIZE; //how many times to read file
            if ((r[i] = dts[i]->length % FILE_BUFFER_SIZE) != 0)
            {
                t[i]++;
            }
            else
            {
                r[i] = FILE_BUFFER_SIZE; //if not, set size to FILE_BUFFER_SIZE for last read
            }

            if (dts[i]->length > maxlen)
            {
                maxidx = i;
                maxlen = dts[i]->length;
            }
        }
        else
        {
            t[i] = r[i] = 0;
        }
    }
    if (maxlen == 0) //all sources are empty.
    {
        DRETURN(0, 1);
    }
    op_malloc(p, cmd->count);
    while (t[maxidx])
    {
        for (i = 0; i < cmd->count; i++)
        {
            memcpy(lp,p,sizeof(unsigned char *) * cmd->count);
            //simple and crude, because I think the condition of too complicated will be slower than this
            bzero(p[i], FILE_BUFFER_SIZE);
            if (t[i])
            {
                read_from(dts[i], p[i], t[i]-- == 1 ? r[i] : FILE_BUFFER_SIZE, offset);
            }
        }
        size = t[maxidx] ? FILE_BUFFER_SIZE : r[maxidx];
        times = size / (sizeof(unsigned long) * 4);
        idx = 0;
        if (times)
        {
            if (cmd->option == AND)
            {
                while (times--)
                {
                    for (i = 1; i < cmd->count; i++)
                    {
                        lp[0][0] &= lp[i][0];
                        lp[0][1] &= lp[i][1];
                        lp[0][2] &= lp[i][2];
                        lp[0][3] &= lp[i][3];
                        lp[i] += 4;
                    }
                    lp[0] += 4;
                    size -= sizeof(unsigned long) * 4;
                    idx += sizeof(unsigned long) * 4;
                }
            }

            else if (cmd->option == OR)
            {
                while (times--)
                {
                    for (i = 1; i < cmd->count; i++)
                    {
                        lp[0][0] |= lp[i][0];
                        lp[0][1] |= lp[i][1];
                        lp[0][2] |= lp[i][2];
                        lp[0][3] |= lp[i][3];
                        lp[i] += 4;
                    }
                    lp[0] += 4;
                    size -= sizeof(unsigned long) * 4;
                    idx += sizeof(unsigned long) * 4;
                }
            }
            else if (cmd->option == XOR)
            {
                while (times--)
                {
                    for (i = 1; i < cmd->count; i++)
                    {
                        lp[0][0] ^= lp[i][0];
                        lp[0][1] ^= lp[i][1];
                        lp[0][2] ^= lp[i][2];
                        lp[0][3] ^= lp[i][3];
                        lp[i] += 4;
                    }
                    lp[0] += 4;
                    size -= sizeof(unsigned long) * 4;
                    idx += sizeof(unsigned long) * 4;
                }
            }
            else if (cmd->option == NOT)
            {
                while (times--)
                {
                    lp[0][0] = ~lp[0][0];
                    lp[0][1] = ~lp[0][1];
                    lp[0][2] = ~lp[0][2];
                    lp[0][3] = ~lp[0][3];
                    lp[0] += 4;
                    size -= sizeof(unsigned long) * 4;
                    idx += sizeof(unsigned long) * 4;
                }
            }
        }
        while (size--)
        {
            if (cmd->option == NOT)
            {
                p[0][idx] = ~p[0][idx];
                idx++;
                continue;
            }
            for (i = 1; i < cmd->count; i++)
            {
                switch (cmd->option)
                {
                case AND:
                    p[0][idx] &= p[i][idx];
                    break;
                case OR:
                    p[0][idx] |= p[i][idx];
                    break;
                case XOR:
                    p[0][idx] ^= p[i][idx];
                    break;
                case NOT:
                    break;
                }
            }
            idx++;
        }
        if (cmd->hook == NONE)
        {
            if (!write_to(dts[-1], p[0], idx, offset))
            {
                op_free(p, cmd->count);
                DRETURN(-1, 1);
            }
        }
        else if (cmd->hook == COUNTOP)
        {
            bits += redisPopcount((void *)p[0], idx);
        }
        offset += idx;
    }
    op_free(p, cmd->count);
    if (cmd->hook == COUNTOP)
    {
        DRETURN(bits, 1);
    }
    DRETURN(offset, 1);
}