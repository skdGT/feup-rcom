#include "data_link.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <number for serial port>\n", argv[0]);
    printf("\nExample: %s 11\t-\tfor '/dev/ttyS11'\n", argv[0]);
    exit(ERROR);
  }

  /* opens transmiter file descriptor on second layer */
  int receiver_fd = llopen(atoi(argv[1]), RECEIVER);
  /* in case there's an error oppening the port */
  if (receiver_fd == ERROR) {
    exit(ERROR);
  }

  char buffer[1024];
  int size;

  int state = 0;

  // * START Control Packet
  while (state == 0) {
    memset(buffer, 0, sizeof(buffer));
    while ((size = llread(receiver_fd, buffer)) == ERROR) {
      printf("Error reading\n");
    }
    control_packet_t packet = parse_control_packet(buffer, size);
    print_control_packet(packet);
    if (packet.control == START) {
      state = 1;
    }
  }

  // * DATA Packets
  int message_size = 0;
  unsigned char *full_message[64 /* max number of packets */];
  for (int i = 0; i < 64; i++) {
    full_message[i] = (unsigned char*) malloc(1024);
  }

  while (state == 1) {
    memset(buffer, 0, sizeof(buffer));
    while ((size = llread(receiver_fd, buffer)) == ERROR) {
      printf("Error reading\n");
    }
    if (buffer[0] == STOP) {
      state = 2;
      break;
    }
    data_packet_t data = parse_data_packet(buffer, size);
    
    if (data.control != DATA) continue;
    
    print_data_packet(&data, FALSE);
    memcpy(full_message[data.sequence], data.data, data.data_field_size);
    message_size++;
  }

  // * STOP Control Packet
  if (state == 2) {
    control_packet_t packet = parse_control_packet(buffer, size);
    print_control_packet(packet);
    printf("Displaying full message\n");
    for (int i = 0; i < message_size; i++) {
      printf("Message[%d]: %s\n", i, full_message[i]);
    }
  }

  /* resets and closes the receiver fd for the port */
  llclose(receiver_fd, RECEIVER);

  return 0;
}
