#include "threads.h"
#include "console.h"
#include "memory.h"
#include "string.h"
#include "files.h"
#include "threads.h"
#include "shapes.h"



#define KEY_PGUP   278
#define KEY_PGDOWN 279
#define KEY_UP 280
#define KEY_DOWN 281
#define KEY_LEFT 282
#define KEY_RIGHT 283
#define SQUARE '#'
#define BORDER ':'

typedef unsigned char uint8_t;

typedef struct 
{
    uint64_t type;
    uint64_t orientation;
    unsigned char colour;
    int x;
    int y;
} block;


block* current_block;
char * videoBuffer;
char *board;

uint64_t random()
{
    uint64_t ret;
    __asm("rdrand %%rax" : "=a"(ret));
    return ret;
}


void block_draw(block* b, char* buffer)
{
    char *video = (buffer+68) + (b->y*160) + (b->x*2);
    char i;
    char i2;

    char* shape = (char*)&shapes[(b->type*4 + b->orientation)*16];

    for (i = 0; i < 4; i++) for (i2 = 0; i2 < 4; i2++)
    {
        if (shape[i*4+i2] != ' ')
        {
            video[(i*160)+(i2*2)]=SQUARE;
            video[(i*160)+(i2*2)+1]=b->colour;
        }
        
        
    }    

}

uint64_t detect_collision(block *b)
{
    uint64_t i,i2;
    char *buf = (board+68) + (b->y*160) + (b->x*2);
    char* shape = (char*)&shapes[(b->type*4 + b->orientation)*16];

    for (i = 0; i < 4; i++) for (i2 = 0; i2 < 4; i2++)
    {
        if (shape[i*4+i2] != ' ') 
        {
            if (buf[(i*160)+(i2*2)] == '@') return 1;
            if (buf[(i*160)+(i2*2)] == SQUARE) return 2;
        }
    }
    return 0;    
}

uint64_t detect_full_lines()
{
    uint64_t i,i2,line,n;
    char *buf = (board+70);
    i = 20;
        
    while(1)
    {
        bool full = true;
        bool empty = true;

        for (i2 = 0; i2 < 10; i2++)
        {
            if (buf[(i*160)+(i2*2)]!=SQUARE)
            {            
                full = false;
            } 
            else
            {
                empty = false;
            }
        }

        if (full)
        {
            for (line = i; line > 1; line--)
            {
                for (n = 0; n < 10; n++)
                {
                    buf[line*160+ (n*2)] = buf[(line-1)*160+ (n*2)];
                    buf[line*160+ (n*2)+1] = buf[(line-1)*160+ (n*2)+1];
                }
            }
            for (n = 0; n < 10; n++)
            {
                buf[160+ (n*2)] = ' '; 
            }
        }
        else
        {
            i--;
            if (i==0) break;
        }

        if (empty) break;

    }
    return 0;    
}

block* block_create()
{
    block *b = (block*)malloc(sizeof(block));
    b->x=4;
    b->y=0;
    b->orientation=0;
    b->type=random() % 7;
    b->colour= random() & 0b111;

    return b;
}

void redraw()
{
    uint64_t i;
    for (i = 0; i < 512; i++) ((uint64_t*)videoBuffer)[i] = ((uint64_t*)board)[i];
    char *video = videoBuffer;
    block_draw(current_block, video);
}

void make_frame()
{
    uint64_t i;
    uint8_t *buf = (uint8_t*)board+68;
    for (i = 0; i < 12; i++) buf[i*2] = (char)'@';
    buf += 160;
    for (i = 0; i < 20; i++)
    {
        buf[0] = '@';
        buf[22] = '@';
        buf+=160;
    }

    for (i = 0; i < 12; i++) buf[i*2] = '@';

    
}




int main(uint64_t param)
{
    uint64_t i;
    uint16_t ch;
    char str[32];
    str[0]=0;
    bool need_redraw = true;

    videoBuffer = get_video_buffer();
    current_block = block_create();
    board = (char*)malloc(4096);
    make_frame();

    char lastSec = ' ';
    while(1)
    {
        getDateTime(str);
        if (str[18] != lastSec)
        {
            lastSec = str[18];
            need_redraw = true;
            current_block->y++;
            uint64_t collision =  detect_collision(current_block);
            if (collision != 0)
            {
                current_block->y--;
                block_draw(current_block, board);
                free(current_block);
                current_block = block_create();
            }
            detect_full_lines();
        }

        if (need_redraw)
        {
            redraw();
            need_redraw = false;
        }

        ch = poll_in();
        if (ch != 0)
        {
            if (ch == 'a') 
            {
                current_block->x--;
                if (detect_collision(current_block))
                {
                    current_block->x++;
                }
                else
                {
                    need_redraw = true;
                }
            }
            if (ch == 's') 
            {
                current_block->x++;
                if (detect_collision(current_block))
                {
                    current_block->x--;
                }
                else
                {
                    need_redraw = true;
                }
            } 
            if (ch == 'x') 
            {
                current_block->y++;
                if (detect_collision(current_block))
                {
                    current_block->y--;
                }
                else
                {
                    need_redraw = true;
                }
            } 
            if (ch == ' ') 
            {
                uint64_t old = current_block->orientation;                
                current_block->orientation = (current_block->orientation+1) & 0b11;
                if (detect_collision(current_block))
                {
                    current_block->orientation = old;
                }
                else
                {
                    need_redraw = true;
                }
            } 
        }
    }

}
