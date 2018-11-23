// crc16.h : контрольная сумма crc16

#ifndef _CRC16_H
#define _CRC16_H

uint16_t crc16(uint8_t *data, uint32_t data_size);

// перевод числа в строку вида "A0EF"
char* crc16_to_string(uint16_t crc16);
// перевод строки вида "A0EF1C..." в число
uint16_t crc16_from_string(uint8_t *str);

//  calculate 16 bits CRC of the given length data. 
uint16_t crc16_itu(const uint8_t* pData, int nLength);

#endif	// _CRC16_H
