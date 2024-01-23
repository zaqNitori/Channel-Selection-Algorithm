#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include <errno.h>
#include <string.h>
#include <pcap/pcap.h>

unsigned char str2int(char* str) {

    char *p;
    int num;
    errno = 0;
    long conv = strtol(str, &p, 10);

    // Check for errors: e.g., the string does not represent an integer
    // or the integer is larger than int
    if (errno != 0 || *p != '\0') {
        // Put here the handling of the error, like exiting the program with
        // an error message
        printf("Convert to int error!\n");
        return 0xff;
    } else {
        num = conv;
    }   
    return num;
}

int main(int argc, char *argv[])
{
    pcap_t *handle;                   /* Session handle */
    char   *dev;                      /* The device to sniff on */
    char   errbuf[PCAP_ERRBUF_SIZE];  /* Error string */

    unsigned char msg_type;  // cs msg type
    unsigned char chan;      // cs switch target channel
    unsigned char cvt;       // convert indicator
    int      write_len;      /* Size of Inject Frame */


    /* check for capture device name on command-line */
	if (argc == 4) {
		dev = argv[1];
	}
	else {
		fprintf(stderr, "error: unrecognized command-line options\n\n");
		return 1;
	}
    
    /* Convert CS msg type */
    cvt = str2int(argv[2]);
    if (cvt == 0xff) {
        printf("CS msg type error!\n");
        return 0;
    }

    switch (cvt)
    {
    case 0:
    // CS Announce
        msg_type = 0x00;
        break;
    case 1:
    // Scan Req
        msg_type = 0x10;
        break;
    case 2:
    // Scan Reply
        msg_type = 0x11;
        break;
    default:
        msg_type = 0xff;
        break;
    }
    
    /* Convert switch target channel */
    cvt = str2int(argv[3]);
    if (cvt == 0xff) {
        printf("CS msg target channel error!\n");
        return 0;
    }
    chan = cvt;


    /* Open the session in promiscuous mode */
    handle = pcap_open_live(dev, BUFSIZ, 1, 100, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
        return(2);
    }

    // radiotap header minimizing
    // Cost 32 bytes.
    unsigned char ieee80211[] = {
        0x00,                                 // Version
        0x00,                                 // Pad
        0x08, 0x00,                           // Header Length (8)
        0x00, 0x00, 0x00, 0x00,               // Present Flags 1
        0x70, 0x00,                           // Frame Control
        0x00, 0x00,                           // Duration ID
        0xff, 0xff, 0xff, 0xff, 0xee, 0xee,   // DA
        0xff, 0xff, 0xff, 0xff, 0xee, 0xee,   // SA
        0xff, 0xff, 0xff, 0xff, 0xee, 0xee,   // BSS ID
        0xb0, 0x22,                           // Seq Number
        //0xff, 0xff, 0xff, 0xff, 0xee, 0xee,   // 
        msg_type,                             // CS msg type
        chan,                                 // target Channel
    };

    write_len = pcap_inject(handle, &ieee80211, sizeof(ieee80211));

    if(write_len == -1) {
        fprintf(stderr, "pcap inject error\n");
        return(2);
    }
    
	/* cleanup */
	pcap_close(handle);
    //free(packet);
	printf("%d bytes Send through %s interface.\n", write_len, dev);    
    
    return(0);
}