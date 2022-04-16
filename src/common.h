#ifndef __COMMON_H__
#define __COMMON_H__

constexpr int TICKET_LEN = 7;
constexpr size_t MAX_CONTENT_SIZE = 65507;
constexpr uint16_t MAX_TICKET_COUNT = (MAX_CONTENT_SIZE - 1 - 4 - 2) / TICKET_LEN;

#endif // __COMMON_H__

