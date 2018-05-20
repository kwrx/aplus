#ifndef __LINKLEVELDRIVER_H
#define __LINKLEVELDRIVER_H

void low_level_init(void *i, uint8_t *addr, void *mcast);
int low_level_startoutput(void *i);
void low_level_output(void *i, void *data, uint16_t len);
void low_level_endoutput(void *i, uint16_t total_len);
int low_level_startinput(void *i);
void low_level_input(void *i, void *data, uint16_t len);
void low_level_endinput(void *i);
void low_level_inputnomem(void *i, uint16_t len);


#endif
