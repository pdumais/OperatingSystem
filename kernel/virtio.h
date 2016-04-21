// Documentation:
// http://ozlabs.org/~rusty/virtio-spec/virtio-0.9.5.pdf, appendix C
// http://docs.oasis-open.org/virtio/virtio/v1.0/cs04/virtio-v1.0-cs04.html
#include "includes/kernel/types.h"

//Virtual I/O Device (VIRTIO) Version 1.0, Spec 4, section 5.1.3:  Feature bits
#define VIRTIO_CSUM                 0       
#define VIRTIO_GUEST_CSUM           1       
#define VIRTIO_CTRL_GUEST_OFFLOADS  2
#define VIRTIO_MAC                  5       
#define VIRTIO_GUEST_TSO4           7       
#define VIRTIO_GUEST_TSO6           8       
#define VIRTIO_GUEST_ECN            9       
#define VIRTIO_GUEST_UFO            10      
#define VIRTIO_HOST_TSO4            11      
#define VIRTIO_HOST_TSO6            12      
#define VIRTIO_HOST_ECN             13      
#define VIRTIO_HOST_UFO             14      
#define VIRTIO_MRG_RXBUF            15      
#define VIRTIO_STATUS               16      
#define VIRTIO_CTRL_VQ              17      
#define VIRTIO_CTRL_RX              18      
#define VIRTIO_CTRL_VLAN            19      
#define VIRTIO_CTRL_RX_EXTRA        20   
#define VIRTIO_GUEST_ANNOUNCE       21  
#define VIRTIO_MQ                   22      
#define VIRTIO_CTRL_MAC_ADDR        23
#define VIRTIO_EVENT_IDX            29

#define VIRTIO_ACKNOWLEDGE 1
#define VIRTIO_DRIVER 2
#define VIRTIO_FAILED 128
#define VIRTIO_FEATURES_OK 8
#define VIRTIO_DRIVER_OK 4
#define VIRTIO_DEVICE_NEEDS_RESET 64

#define VIRTIO_DESC_FLAG_NEXT           1 
#define VIRTIO_DESC_FLAG_WRITE_ONLY     2 
#define VIRTIO_DESC_FLAG_INDIRECT       4 

#define VIRTIO_NET_HDR_F_NEEDS_CSUM    1
#define VIRTIO_NET_HDR_GSO_NONE        0 
#define VIRTIO_NET_HDR_GSO_TCPV4       1 
#define VIRTIO_NET_HDR_GSO_UDP         3 
#define VIRTIO_NET_HDR_GSO_TCPV6       4 
#define VIRTIO_NET_HDR_GSO_ECN         0x80 

#define PAGE_COUNT(x) ((x+0xFFF)>>12) 
#define DISABLE_FEATURE(v,feature) v &= ~(1<<feature)
#define ENABLE_FEATURE(v,feature) v |= (1<<feature)
#define HAS_FEATURE(v,feature) (v & (1<<feature))

typedef struct
{
    u8 flags;
    u8 gso_type;
    u16 header_length;
    u16 gso_size;
    u16 checksum_start;
    u16 checksum_offset;
} net_header;

typedef struct
{
    u64 address;
    u32 length;
    u16 flags;
    u16 next;
} queue_buffer;

typedef struct
{
    u16 flags;
    u16 index;
    u16 rings[];
} virtio_available;

typedef struct
{
    u32 index;
    u32 length;
} virtio_used_item;

typedef struct
{
    u16 flags;
    u16 index;
    virtio_used_item rings[];
} virtio_used;

typedef struct
{
    u16 queue_size;
    union
    {
        queue_buffer* buffers;
        u64 baseAddress;
    };
    virtio_available* available;
    virtio_used* used;
    u16 last_used_index;
    u16 last_available_index;
} virt_queue;

struct virtio_device_info
{
    unsigned int deviceAddress;
    unsigned short iobase;
    unsigned long memoryAddress;
    unsigned short irq;
    unsigned long macAddress;
    virt_queue queues[16];
};


typedef struct
{
    u32 index;
    u64 address;
    u32 length;
} send_buffer;

bool virtio_init(struct virtio_device_info* dev, void (*negotiate)(u32* features));
bool virtio_queue_setup(struct virtio_device_info* dev, unsigned char index);
send_buffer virtio_get_send_buffer(virt_queue* vq, u32 size);
void virtio_send_buffer_ready(struct virtio_device_info* dev, u16 queue_index, u16 index);
void virtio_clean_used_buffers(struct virtio_device_info* dev, u16 queue_index);
void virtio_set_next_receive_buffer_available(virt_queue* vq, u16 count);
