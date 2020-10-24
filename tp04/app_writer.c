#include "data_link.h"
#include "files.h"

extern int flag;
FILE *fp;
file_t file;

control_packet_t generate_control_packet(int control) {
  control_packet_t c_packet;
  c_packet.control = control;
  c_packet.file_name = file.name;  

  unsigned char buf[sizeof(unsigned long)];
  int num = number_to_array(file.size, buf);

  c_packet.file_size = (unsigned char *)malloc(num);
  memcpy(c_packet.file_size, buf, num);
  c_packet.filesize_size = num;

  int i = 0;
  // control packet
  c_packet.raw_bytes = (unsigned char *)malloc(i + 1);
  c_packet.raw_bytes[i++] = c_packet.control;
  c_packet.raw_bytes = (unsigned char *)realloc(c_packet.raw_bytes, (i + 1));
  // file size
  c_packet.raw_bytes[i++] = FILE_SIZE;
  c_packet.raw_bytes = (unsigned char *)realloc(c_packet.raw_bytes, (i + 1));
  c_packet.raw_bytes[i++] = c_packet.filesize_size;

  for (int j = 0; j < c_packet.filesize_size; j++) {
    c_packet.raw_bytes = (unsigned char *)realloc(c_packet.raw_bytes, (i + 1));
    c_packet.raw_bytes[i++] = c_packet.file_size[j];
  }
  c_packet.raw_bytes = (unsigned char *)realloc(c_packet.raw_bytes, (i + 1));
  // file name
  c_packet.raw_bytes[i++] = FILE_NAME;
  c_packet.raw_bytes = (unsigned char *)realloc(c_packet.raw_bytes, (i + 1));
  c_packet.raw_bytes[i++] = strlen(c_packet.file_name);
  c_packet.raw_bytes = (unsigned char *)realloc(c_packet.raw_bytes, (i + 1));
  for (int j = 0; j < strlen(c_packet.file_name); j++) {
    c_packet.raw_bytes[i++] = c_packet.file_name[j];
    c_packet.raw_bytes = (unsigned char *)realloc(c_packet.raw_bytes, (i + 1));
  }

  c_packet.raw_bytes_size = i;

  return c_packet;
}

data_packet_t generate_data_packet(unsigned char *buffer, int size, int sequence) {
  data_packet_t d_packet;
  d_packet.control = DATA;
  d_packet.data_field_size = size;
  d_packet.sequence = sequence;

  int i = 0;
  // control
  d_packet.raw_bytes = (unsigned char *)malloc(i + 1);
  d_packet.raw_bytes[i++] = d_packet.control;
  d_packet.raw_bytes = (unsigned char *)realloc(d_packet.raw_bytes, (i + 1));
  // sequence
  d_packet.raw_bytes[i++] = d_packet.sequence;
  d_packet.raw_bytes = (unsigned char *)realloc(d_packet.raw_bytes, (i + 1));
  // size
  unsigned int x = (unsigned int)size;
  unsigned char high = (unsigned char)(x >> 8);
  unsigned char low = x & 0xff;
  d_packet.raw_bytes[i++] = high;
  d_packet.raw_bytes = (unsigned char *)realloc(d_packet.raw_bytes, (i + 1));
  d_packet.raw_bytes[i++] = low;
  d_packet.raw_bytes = (unsigned char *)realloc(d_packet.raw_bytes, (i + 1));
  // data
  for (int j = 0; j < size; j++) {
    d_packet.data[j] = buffer[j];
    d_packet.raw_bytes[i++] = buffer[j];
    d_packet.raw_bytes = (unsigned char *)realloc(d_packet.raw_bytes, (i + 1));
  }

  d_packet.raw_bytes_size = i;

  return d_packet;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <number for serial port>\n", argv[0]);
    printf("\nExample: %s 11\t-\tfor '/dev/ttyS11'\n", argv[0]);
    return -1;
  }

  /* opens transmiter file descriptor on second layer */
  int transmiter_fd = llopen(atoi(argv[1]), TRANSMITTER);
  /* in case there's an error oppening the port */
  if (transmiter_fd == ERROR) {
    exit(ERROR);
  }

  fp = fopen("pinguim.gif", "rb");
  file.name = "pinguim.gif";
  file.size = get_file_size(fp);
  file.data = read_file(fp, file.size);

  control_packet_t c_packet_start = generate_control_packet(START);
  control_packet_t c_packet_stop = generate_control_packet(STOP);

  // sending control packet
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  print_control_packet(c_packet_start);

  int size = llwrite(transmiter_fd, c_packet_start.raw_bytes, c_packet_start.raw_bytes_size);
  if (size == ERROR) {
    printf("Error writing START Control Packet, aborting...\n");
    llclose(transmiter_fd, TRANSMITTER);
    return ERROR;
  }
  print_elapsed_time(start);

  unsigned long bytes_left = file.size;
  int index_start;
  int index_end = -1;
  int sequence = 0;
  while (bytes_left != 0 && index_end != file.size - 1) {
    usleep(STOP_AND_WAIT);

    index_start = index_end + 1;
    if (bytes_left >= 1023) {
      index_end = index_start + 1023;
    } else {
      index_end = index_start + bytes_left - 1;
    }
    bytes_left -= (index_end - index_start) + 1;

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

    data_packet_t data = generate_data_packet(split_file(file.data, index_start, index_end), index_end - index_start + 1, sequence++);
    print_data_packet(&data, FALSE);

    size = llwrite(transmiter_fd, data.raw_bytes, data.raw_bytes_size);
    if (size == ERROR) {
      printf("Error writing Data Packet, aborting...\n");
      llclose(transmiter_fd, TRANSMITTER);
      return ERROR;
    }
    print_elapsed_time(start);
  }

  usleep(STOP_AND_WAIT);
  print_control_packet(c_packet_stop);
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  size = llwrite(transmiter_fd, c_packet_stop.raw_bytes, c_packet_stop.raw_bytes_size);
  if (size == ERROR) {
    printf("Error writing STOP Control Packet, aborting...\n");
    llclose(transmiter_fd, TRANSMITTER);
    return ERROR;
  }

  usleep(STOP_AND_WAIT);
  print_elapsed_time(start);
  /* resets and closes the receiver fd for the port */
  llclose(transmiter_fd, TRANSMITTER);

  return OK;
}
