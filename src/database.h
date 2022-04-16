#ifndef __TICKET_DATABASE_H__
#define __TICKET_DATABASE_H__

#include "common.h"

#include <cstdint>
#include <exception>
#include <string>
#include <unordered_map>
#include <vector>
#include <queue>

//////////////////////////
//                      //
//      CONTSTANTS      //
//                      //
//////////////////////////


constexpr int COOKIE_LEN = 48;


///////////////////////////
///                     ///
///     EXCEPTIONS      ///
///                     ///
///////////////////////////


class EventNotFound : public std::exception {
    virtual const char *what() const noexcept;
};

class ReservationNotFound : public std::exception {
    virtual const char *what() const noexcept;
};

class InvalidTicketCount : public std::exception {
    virtual const char *what() const noexcept;
};

class TicketShortage : public std::exception {
    virtual const char *what() const noexcept;
};

class TooManyTickets : public std::exception {
    virtual const char *what() const noexcept;
};

class InvalidReservationID : public std::exception {
    virtual const char *what() const noexcept;
};

class InvalidCookie : public std::exception {
    virtual const char *what() const noexcept;
};


///////////////////////////
///                     ///
///     AUXILIARY       ///
///      STRUCTS        ///
///                     ///
///////////////////////////


struct Event {
    uint32_t            event_id;
    const std::string   description;
    uint16_t            ticket_count;

    Event(uint32_t event_id_, std::string &&description_, uint16_t ticket_count_);
    Event(uint32_t event_id_, const std::string &description_, uint16_t ticket_count_);
    ~Event() = default;
};

struct Reservation {
    uint32_t    reservation_id;
    uint32_t    event_id;
    uint16_t    ticket_count;
    char        cookie[COOKIE_LEN];
    uint64_t    expiration_time;

    Reservation() = delete;
    Reservation(uint32_t reservation_id_, uint32_t event_id_,
                uint16_t ticket_count_, uint64_t expiration_time_);

private:
    void generate_cookie();
};

struct Ticket {
    char code[TICKET_LEN];
};


///////////////////////////
///                     ///
///      DATABASE       ///
///                     ///
///////////////////////////


class Database {
/* Types */
private:
    struct ReservationInfo;
    struct ReservationTime;

public:
    class event_iterator : public std::vector<Event>::const_iterator {
    public:
        event_iterator() = default;
        event_iterator(const std::vector<Event>::const_iterator &other)
        : std::vector<Event>::const_iterator{other} {}
        ~event_iterator() = default;
    };

/* Fields */
private:
    const uint64_t                                  timeout;
    std::vector<Event>                              events;
    std::unordered_map<uint32_t, ReservationInfo>   reservations;
    std::queue<ReservationTime>                     reservation_queue;
    uint32_t                                        next_reservation_id;
    char                                            base_ticket[TICKET_LEN];

/* Methods */
public:
    Database() = delete;
    Database(uint64_t timeout_);
    ~Database() = default;

    void add_event(std::string &&description, uint16_t ticket_count);
    void add_event(const std::string &description, uint16_t ticket_count);

    event_iterator events_begin() const noexcept {
        return events.cbegin();
    }

    event_iterator events_end() const noexcept {
        return events.cend();
    }

    // can throw
    Reservation make_reservation(uint32_t event_id, uint16_t ticket_count);
    // can throw
    [[nodiscard]] std::vector<Ticket> get_tickets(uint32_t reservation_id, char const *cookie);

private:
    // can throw
    uint32_t get_reservation_id();
    void remove_reservation(const uint32_t reservation_id) noexcept;
    void clean_queue() noexcept;
    void generate_tickets(ReservationInfo &reservation, uint16_t ticket_count) noexcept;
};


#endif // __TICKET_DATABASE_H__

