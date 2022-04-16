#include "database.h"

#include <cstring> // memcpy
#include <chrono>  // time


///////////////////////////
///                     ///
///      CONSTANTS      ///
///                     ///
///////////////////////////


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
constexpr uint32_t MIN_RESERVATION_ID = 10e6;


///////////////////////////
///                     ///
///     EXCEPTIONS      ///
///                     ///
///////////////////////////


const char *EventNotFound::what() const noexcept {
    return "The event does not exist.";
}

const char *ReservationNotFound::what() const noexcept {
    return "The reservation does not exist.";
}

const char *InvalidTicketCount::what() const noexcept {
    return "The provided ticket count is invalid.";
}

const char *TicketShortage::what() const noexcept {
    return "Too few tickets available.";
}

const char *TooManyTickets::what() const noexcept {
    return "The number of tickets will not be able to be stored in a single datagram.";
}

const char *InvalidReservationID::what() const noexcept {
    return "Invalid reservation ID.";
}

const char *InvalidCookie::what() const noexcept {
    return "Invalid cookie.";
}


///////////////////////////
///                     ///
///     AUXILIARY       ///
///      STRUCTS        ///
///                     ///
///////////////////////////


Event::Event(uint32_t event_id_, std::string &&description_, uint16_t ticket_count_)
: event_id{event_id_}
, description{std::move(description_)}
, ticket_count{ticket_count_} {}

Event::Event(uint32_t event_id_, const std::string &description_, uint16_t ticket_count_)
: event_id{event_id_}
, description{description_}
, ticket_count{ticket_count_} {}

Reservation::Reservation(uint32_t reservation_id_, uint32_t event_id_,
                         uint16_t ticket_count_, uint64_t expiration_time_)
: reservation_id{reservation_id_}
, event_id{event_id_}
, ticket_count{ticket_count_}
, expiration_time{expiration_time_}
{
    generate_cookie();
}

void Reservation::generate_cookie() {
    for (int i = 0; i < COOKIE_LEN / 2; ++i)
        cookie[i] = reservation_id % SMALL_PRIMES[i] + MIN_COOKIE_CHAR;

    for (int i = COOKIE_LEN / 2; i < COOKIE_LEN; ++i)
        cookie[i] = ((reservation_id + 1) * PRIMES[i]) % SMALL_PRIMES[i - COOKIE_LEN / 2]
                    + MIN_COOKIE_CHAR;
}


///////////////////////////
///                     ///
///     AUXILIARY       ///
///     FUNCTIONS       ///
///                     ///
///////////////////////////


namespace {
    uint64_t get_seconds_from_epoch() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }

    bool cmp_cookies(char const *cookie1, char const *cookie2) noexcept {
        for (int i = 0; i < COOKIE_LEN; ++i)
            if (cookie1[i] != cookie2[i])
                return false;
        return true;
    }

    void increase_ticket(char *ticket, uint16_t difference) noexcept {
        auto to_int = [](char c) { return (c < 'A') ? c - '0' : c - 'A' + 10; };
        auto to_char = [](int x) { return (x > 9) ? 'A' + x - 10 : '0' + x; };

        const int offset = '9' - '0' + 1 + 'Z' - 'A' + 1;

        for (int i = 0; i < TICKET_LEN; ++i) {
            int value = difference % offset + to_int(ticket[i]);
            difference /= offset;
            if (value > offset) {
                ++ticket[i + 1];
                value -= offset;
            }
            ticket[i] = to_char(value);
        }
    }
}


///////////////////////////
///                     ///
///      DATABASE       ///
///                     ///
///////////////////////////


struct Database::ReservationInfo {
    uint32_t    event_id;
    uint16_t    ticket_count;
    char        cookie[COOKIE_LEN];
    char        ticket_min[TICKET_LEN];
    bool        received = false;

    ReservationInfo(const Reservation &reservation)
    : event_id{reservation.event_id}
    , ticket_count{reservation.ticket_count}
    {
        memcpy(cookie, reservation.cookie, COOKIE_LEN);
    }

    ~ReservationInfo() = default;
};

struct Database::ReservationTime {
    uint32_t reservation_id;
    uint64_t expiration_time;

    ReservationTime(uint32_t reservation_id_, uint64_t expiration_time_)
    : reservation_id{reservation_id_}
    , expiration_time{expiration_time_} {}

    ~ReservationTime() = default;
};



Database::Database(uint64_t timeout_)
: timeout{timeout_}
, next_reservation_id{MIN_RESERVATION_ID}
{
    memset(base_ticket, '0', TICKET_LEN);
}

void Database::add_event(std::string &&description, uint16_t ticket_count) {
    events.push_back(Event(events.size(), std::move(description), ticket_count));
}

void Database::add_event(const std::string &description, uint16_t ticket_count) {
    events.push_back(Event(events.size(), description, ticket_count));
}

// can throw
Reservation Database::make_reservation(uint32_t event_id, uint16_t ticket_count) {
    if (!ticket_count)
        throw InvalidTicketCount();
    if (ticket_count > MAX_TICKET_COUNT)
        throw TooManyTickets();
    if (event_id >= events.size())
        throw EventNotFound();
    if (events[event_id].ticket_count < ticket_count)
        throw TicketShortage();
        
    const uint64_t expiration_time = get_seconds_from_epoch() + timeout;
    const uint32_t reservation_id = get_reservation_id();
    events[event_id].ticket_count -= ticket_count;

    Reservation result(reservation_id, event_id, ticket_count, expiration_time);
    ReservationInfo info(result);
    generate_tickets(info, ticket_count);

    reservations.emplace(reservation_id, info);
    reservation_queue.push(ReservationTime(reservation_id, expiration_time));
        
    return result;
}

// can throw
[[nodiscard]] std::vector<Ticket>
Database::get_tickets(uint32_t reservation_id, char const *cookie) {
    clean_queue();
        
    try {
        auto &reservation = reservations.at(reservation_id);
        if (!cmp_cookies(cookie, reservation.cookie))
            throw InvalidCookie();

        reservation.received = true;
        std::vector<Ticket> result(reservation.ticket_count);
        memcpy(result[0].code, reservation.ticket_min, TICKET_LEN);
        for (uint16_t i = 1; i < reservation.ticket_count; ++i) {
            memcpy(result[i].code, result[i - 1].code, TICKET_LEN);
            increase_ticket(result[i].code, 1);
        }
        return result;
    } catch (std::out_of_range&) {
        throw ReservationNotFound();
    }
}

// can throw
uint32_t Database::get_reservation_id() {
    if (next_reservation_id + 1 < MIN_RESERVATION_ID)
        throw InvalidReservationID();
    return next_reservation_id++;
}

void Database::remove_reservation(const uint32_t reservation_id) noexcept {
    try {
        const auto &record = reservations.at(reservation_id);
        if (record.received)
            return;
        events[record.event_id].ticket_count += record.ticket_count;
        reservations.erase(reservation_id);
    } catch (...) {
        // ignore
    }
}

void Database::clean_queue() noexcept {
    const uint64_t current_time = get_seconds_from_epoch();
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

void Database::generate_tickets(ReservationInfo &reservation, uint16_t ticket_count) noexcept {
    memcpy(reservation.ticket_min, base_ticket, TICKET_LEN);
    increase_ticket(base_ticket, ticket_count);
}

