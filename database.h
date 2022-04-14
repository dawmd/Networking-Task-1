#ifndef __MESSAGE_SERVER_DATABASE__
#define __MESSAGE_SERVER_DATABASE__

#include <string>
#include <unordered_map>
#include <vector>
#include <queue>

#include <chrono>
#include <cstring> // for memcpy
#include <exception>


///////////////////////////
///                     ///
///     CONSTANTS       ///
///                     ///
///////////////////////////


constexpr uint32_t MIN_RESERVATION_ID = 10e6;
constexpr int COOKIE_LEN = 48;

constexpr uint64_t PRIMES[COOKIE_LEN] = {
    15485863,  49979687,  86028121,
    104395303, 122949829, 160481183,
    160481219, 198491317, 198491329,
    236887691, 256203161, 256203221,
    295075147, 295075153, 314606869,
    314606891, 334214459, 334214467,
    353868013, 353868019, 373587883,
    373587911, 393342739, 393342743,
    413158511, 413158523, 433024223,
    433024253, 452930459, 452930477,
    472882027, 472882049, 492876847,
    492876863, 512927357, 512927377,
    533000389, 533000401, 553105243,
    553105253, 573259391, 573259433,
    593441843, 593441861, 613651349,
    613651369, 633910099, 633910111
};

constexpr uint64_t SMALL_PRIMES[48 / 2] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41,
    43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89
};

constexpr char MIN_COOKIE_CHAR = 33;

constexpr uint64_t DEFAULT_TIMEOUT = 5;

constexpr int TICKET_LEN = 7;

///////////////////////////
///                     ///
///     EXCEPTIONS      ///
///                     ///
///////////////////////////


class EventNotFound : public std::exception {
    virtual const char *what() const noexcept {
        return "The event does not exist.";
    }
};

class ReservationNotFound : public std::exception {
    virtual const char *what() const noexcept {
        return "The reservation does not exist.";
    }
};

class InvalidTicketCount : public std::exception {
    virtual const char *what() const noexcept {
        return "The provided ticket count is invalid.";
    }
};

class TicketShortage : public std::exception {
    virtual const char *what() const noexcept {
        return "Too few tickets available.";
    }
};

class InvalidReservationID : public std::exception {
    virtual const char *what() const noexcept {
        return "Invalid reservation ID.";
    }
};

class InvalidCookie : public std::exception {
    virtual const char *what() const noexcept {
        return "Invalid cookie.";
    }
};


///////////////////////////
///                     ///
///     DATABASE        ///
///                     ///
///////////////////////////

struct Event {
    uint32_t event_id;
    const std::string description;
    uint16_t ticket_count;

    Event(uint32_t event_id_, std::string &&description_, uint16_t ticket_count_)
    : event_id(event_id_)
    , description(std::move(description_))
    , ticket_count(ticket_count_) {}

    Event(uint32_t event_id_, const std::string &description_, uint16_t ticket_count_)
    : event_id(event_id_)
    , description(description_)
    , ticket_count(ticket_count_) {}

    ~Event() = default;
};

struct Reservation {
    uint32_t reservation_id;
    uint32_t event_id;
    uint16_t ticket_count;
    char cookie[COOKIE_LEN];
    uint64_t expiration_time;

    Reservation() = delete;
    Reservation(uint32_t reservation_id_, uint32_t event_id_, uint16_t ticket_count_, uint64_t expiration_time_)
    : reservation_id(reservation_id_)
    , event_id(event_id_)
    , ticket_count(ticket_count_)
    , expiration_time(expiration_time_)
    {
        generate_cookie();
    }

private:
    void generate_cookie() {
        for (int i = 0; i < COOKIE_LEN / 2; ++i)
            cookie[i] = reservation_id % SMALL_PRIMES[i] + MIN_COOKIE_CHAR;
        for (int i = COOKIE_LEN / 2; i < COOKIE_LEN; ++i)
            cookie[i] = (reservation_id * PRIMES[i]) % SMALL_PRIMES[i - COOKIE_LEN / 2] + MIN_COOKIE_CHAR;
    }
};

struct Ticket {
    char code[TICKET_LEN];
};

class Database {
/* Types */
private:
    struct ReservationInfo {
        uint32_t event_id;
        uint16_t ticket_count;
        char cookie[COOKIE_LEN];

        // ReservationInfo(uint32_t event_id_, uint16_t ticket_count_, char const *cookie_)
        // : event_id(event_id_)
        // , ticket_count(ticket_count_)
        // {
        //     memcpy(cookie, cookie_, COOKIE_LEN);
        // }

        ReservationInfo(const Reservation &reservation)
        : event_id(reservation.event_id)
        , ticket_count(reservation.ticket_count)
        {
            memcpy(cookie, reservation.cookie, COOKIE_LEN);
        }

        ~ReservationInfo() = default;
    };

    struct ReservationTime {
        uint32_t reservation_id;
        uint64_t expiration_time;

        ReservationTime(uint32_t reservation_id_, uint64_t expiration_time_)
        : reservation_id(reservation_id_)
        , expiration_time(expiration_time_) {}

        ~ReservationTime() = default;
    };

public:
    class event_iterator : public std::vector<Event>::const_iterator {
    public:
        event_iterator() = default;
        event_iterator(const std::vector<Event>::const_iterator &other)
        : std::vector<Event>::const_iterator(other) {}
        ~event_iterator() = default;
    };

/* Fields */
private:
    const uint64_t timeout;
    std::vector<Event> events;
    std::unordered_map<uint32_t, ReservationInfo> reservations;
    std::queue<ReservationTime> reservation_queue;
    uint32_t next_reservation_id = MIN_RESERVATION_ID;
    char base_ticket[TICKET_LEN];

/* Methods */
public:
    Database(uint64_t timeout_ = DEFAULT_TIMEOUT)
    : timeout(timeout_)
    {
        memset(base_ticket, '0', TICKET_LEN);
    }
    ~Database() = default;

    void add_event(std::string &&description, uint16_t ticket_count) {
        events.push_back(Event(events.size(), std::move(description), ticket_count));
    }

    void add_event(const std::string &description, uint16_t ticket_count) {
        events.push_back(Event(events.size(), description, ticket_count));
    }

    event_iterator events_begin() const noexcept {
        return events.cbegin();
    }

    event_iterator events_end() const noexcept {
        return events.cend();
    }

    // can throw
    Reservation make_reservation(uint32_t event_id, uint16_t ticket_count) {
        if (!ticket_count)
            throw InvalidTicketCount();
        if (event_id >= events.size())
            throw EventNotFound();
        if (events[event_id].ticket_count < ticket_count)
            throw TicketShortage();
        
        const uint64_t expiration_time = get_seconds_from_epoch() + timeout;
        const uint32_t reservation_id = get_reservation_id();
        events[event_id].ticket_count -= ticket_count;

        Reservation result(reservation_id, event_id, ticket_count, expiration_time);

        reservations.emplace(reservation_id, ReservationInfo(result));
        reservation_queue.push(ReservationTime(reservation_id, expiration_time));
        
        return result;
    }

    // can throw
    std::vector<Ticket> get_tickets(uint32_t reservation_id, char const *cookie) {
        clean_queue(get_seconds_from_epoch());
        
        auto &reservation = reservations.at(reservation_id);
        if (!cmp_cookies(cookie, reservation.cookie))
            throw InvalidCookie();

        std::vector<Ticket> result(reservation.ticket_count);
        for (auto &ticket : result)
            generate_ticket(ticket);
        return result;
    }

private:
    static uint64_t get_seconds_from_epoch() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }
    
    static bool cmp_cookies(char const *x, char const *y) noexcept {
        for (int i = 0; i < COOKIE_LEN; ++i)
            if (x[i] != y[i])
                return false;
        return true;
    }

    // can throw
    uint32_t get_reservation_id() {
        if (next_reservation_id + 1 < MIN_RESERVATION_ID)
            throw InvalidReservationID();
        return next_reservation_id++;
    }

    void remove_reservation(const uint32_t reservation_id) noexcept {
        try {
            const auto &record = reservations.at(reservation_id);
            events[record.event_id].ticket_count += record.ticket_count;
            reservations.erase(reservation_id);
        } catch (...) {
            // ignore
        }
    }

    // why do I pass current_time as an argument??
    void clean_queue(const uint64_t current_time) noexcept {
        while (!reservation_queue.empty()) {
            auto &top = reservation_queue.front();
            if (top.expiration_time < current_time) {
                remove_reservation(top.reservation_id);
                reservation_queue.pop();
            } else {
                break;
            }
        }
    }

    static char next_char(char c) noexcept {
        if (c >= '0' && c < '9')
            return ++c;
        if (c == '9')
            return 'A';
        if (c >= 'A' && c < 'Z')
            return ++c;
        return '0';
    }

    void generate_ticket(Ticket &ticket) noexcept {
        for (int i = 0; i < TICKET_LEN; ++i) {
            base_ticket[i] = next_char(base_ticket[i]);
            if (base_ticket[i] != '0')
                break;
        }
        memcpy(ticket.code, base_ticket, TICKET_LEN);
    }
};


#endif // __MESSAGE_SERVER_DATABASE__
