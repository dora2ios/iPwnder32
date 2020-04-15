#ifndef payload_gen_H
#define payload_gen_H

int get_payload_configuration(int cpid, char* identifier, unsigned char** payload, size_t* payload_len);
int gen_limera1n(int cpid, int rom, unsigned char** payload, size_t* payload_len);

#endif
