#include <stdint.h>

#define CE0 TIN2
#define CE1 TXD
#define CE2 T0

void ch55x_spimaster_init();
void ch55x_spimaster_assert_cs(uint8_t device);
void ch55x_spimaster_dessert_cs(uint8_t device);
uint8_t ch55x_spimaster_transfer(uint8_t data);

