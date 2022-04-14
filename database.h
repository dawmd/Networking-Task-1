#include <iostream>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <queue>
#include <optional>
#include <chrono>
#include <exception>
#include <cstring> // memcpy
#include <ctime>

constexpr uint64_t DEFAULT_TIMEOUT = 5;

constexpr int COOKIE_LEN = 48;
constexpr int MAX_DESCRIPTION_LEN = 80;
constexpr int TICKET_LEN = 7;

constexpr uint32_t MIN_RESERVATION_ID = 10e6;
constexpr uint32_t MAX_EVENT_ID = 10e6 - 1;

// constexpr uint64_t BIG_PRIME = 982451653;
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

struct Reservation {
    uint32_t reservation_id;
    uint32_t event_id;
    uint16_t ticket_count;
    // we don't care about a null character at the end
    // since it's not going to be part of the message,
    // so it's just COOKIE_LEN, without the `+1` thing
    char cookie[COOKIE_LEN] = { 0 };
    uint64_t expiration_time;

    Reservation() = delete;

    Reservation(uint32_t reservation_id_, uint32_t event_id_, char *cookie_, uint16_t ticket_count_, uint64_t expiration_time_)
    : reservation_id(reservation_id_)
    , event_id(event_id_)
    , ticket_count(ticket_count_)
    , expiration_time(expiration_time_)
    {
        memcpy(cookie, cookie_, COOKIE_LEN);
    }

    ~Reservation() = default;
};

struct Ticket {
    char ticket[TICKET_LEN] = { 0 }; // digits and uppercase English letters
};

struct Tickets {
    uint32_t reservation_id;
    uint16_t ticket_count;
    Ticket *tickets = nullptr;
};

class Database {
private:
    struct Event {
        // uint32_t event_id;
        char description[MAX_DESCRIPTION_LEN + 1] = { 0 };
        // HERE should go the lenght of the description
        // Keep in mind to add this information for the iterator
        uint8_t description_length;
        uint16_t ticket_count;
    };

    struct ReservationInfo {
        uint32_t event_id;
        uint16_t ticket_count;
        char cookie[COOKIE_LEN + 1] = { 0 };

        ReservationInfo() = delete;
        ReservationInfo(uint32_t event_id_, uint16_t ticket_count_)
        : event_id(event_id_)
        , ticket_count(ticket_count_) {}
        ~ReservationInfo() = default;

        // It's relatively easy to prove that for two cookies are equal
        // if and only if provided codes to generate them were the same
        // number. It relies on the fact these numbers are from a finite
        // (and relatively small) set.
        void generate_cookie(uint64_t code) {
            for (int i = 0; i < COOKIE_LEN; ++i) {
                cookie[i] = (PRIMES[i] * code) % SMALL_PRIMES[i / 2] + MIN_COOKIE_CHAR;
            }
        }
    };

    struct ReservationTime {
        uint32_t reservation_id;
        uint64_t expiration_time;

        ReservationTime() = delete;
        ReservationTime(uint32_t reservation_id_, uint64_t expiration_time_)
        : reservation_id(reservation_id_)
        , expiration_time(expiration_time_) {}
        ~ReservationTime() = default;
    };

    const uint64_t timeout;
    std::unordered_map<uint32_t, Event> events; // can be an std::vector
    std::unordered_map<uint32_t, ReservationInfo> reservations;
    std::queue<ReservationTime> reservation_queue;
    uint32_t free_reservation_id = MIN_RESERVATION_ID;
    std::queue<uint32_t> unused_reservation_ids;

public:
    struct EventInfo {
        uint32_t event_id;
        char const *description;
        uint8_t description_length;
        uint16_t ticket_count;

        EventInfo() = delete;
        EventInfo(uint32_t event_id_, char const *description_, uint8_t description_length_, uint16_t ticket_count_)
        : event_id(event_id_)
        , description(description_)
        , description_length(description_length_)
        , ticket_count(ticket_count_) {}
        ~EventInfo() = default;
    };

private:
    using event_map = std::unordered_map<uint32_t, Event>;

public:
    class event_iterator : protected event_map::const_iterator {
    private:
        using super = event_map::const_iterator;
    
    public:
        event_iterator() = default;
        
        event_iterator(const event_map::const_iterator& other)
        : event_map::const_iterator(other)
        {}
        
        ~event_iterator() = default;

        event_iterator &operator++() noexcept {
            super::operator++();
            return *this;
        }

        bool operator==(const event_iterator &other) const noexcept {
            return static_cast<super>(*this) == static_cast<super>(other);
        }

        bool operator!=(const event_iterator &other) const noexcept {
            return !(*this == other);
        }

        [[nodiscard]] EventInfo get() const noexcept {
            const auto &info = *static_cast<super>(*this);
            return EventInfo(
                info.first,
                info.second.description,
                info.second.description_length,
                info.second.ticket_count
            );
        }
    };

public:
    Database() = delete;
    Database(uint64_t timeout_ = DEFAULT_TIMEOUT)
    : timeout(timeout_) {}
    ~Database() = default;

    event_iterator events_begin() {
        return events.cbegin();
    }

    event_iterator events_end() {
        return events.cend();
    }

    std::optional<Reservation> get_reservation(uint32_t event_id, uint16_t ticket_count) {
        if (!ticket_count) {
            std::cerr << "Error. Tried to book 0 tickets for an event.\n";
            return {};
        }
        const uint64_t current_time = get_seconds_from_epoch();
        const uint64_t expiration_time = current_time + timeout;

        try {
            auto &event = events.at(event_id);
            clean_queue(current_time);
            if (event.ticket_count < ticket_count) {
                std::cerr << "The number of tickets available for the event of ID equal to "
                          << event_id << " is too small.\n";
                return {};
            }
            event.ticket_count -= ticket_count;
            return { make_reservation(event_id, ticket_count, expiration_time) };
        } catch (std::exception &e) {
            std::cerr << "The event of ID equal to " << event_id << " does not exist.\n";
            // the event does not exist, bad request
            return {};
        }
    }

    std::optional<Tickets> get_tickets(uint32_t reservation_id, char const *cookie) {
        const uint64_t current_time = get_seconds_from_epoch();
        clean_queue(current_time);
        try {
            auto reservation = reservations.at(reservation_id);
            if (!cmp_cookies(reservation.cookie, cookie)) {
                std::cerr << "Passed cookie does not match the actual one.\n";
                return {};
            }
            reservations.erase(reservation_id);
            return confirm_reservation(reservation_id, reservation);
        } catch (std::exception &e) {
            std::cerr << "The reservation of ID equal to " << reservation_id << " does not exist.\n";
            // the reservation does not exist, bad request
            return {};
        }
    }

private:
    static uint64_t get_seconds_from_epoch() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }
    
    static bool cmp_cookies(char const *s1, char const *s2) {
        for (int i = 0; i < COOKIE_LEN; ++i)
            if (s1[i] != s2[i])
                return false;
        return true;
    }

    static std::optional<Tickets> confirm_reservation(uint32_t reservation_id, const ReservationInfo &info) {
        Tickets result;
        result.ticket_count = info.ticket_count;
        try {
            result.tickets = new Ticket[info.ticket_count];
        } catch (std::bad_alloc &e) {
            std::cerr << "Allocating memory has failed.\n";
            return {};
        }

        for (uint16_t i = 0; i < info.ticket_count; ++i) {
            
        }
    }

    uint32_t get_reservation_id() {
        if (free_reservation_id + 1 < MIN_RESERVATION_ID) {
            throw std::exception(); // TODO
        }
        return free_reservation_id++;
    }

    Reservation make_reservation(const uint32_t event_id, const uint16_t ticket_count, const uint64_t expiration_time) {
        uint32_t reservation_id = get_reservation_id(); // CAN throw an exception
        reservation_queue.push(ReservationTime(reservation_id, expiration_time));
        ReservationInfo info(event_id, ticket_count);
        info.generate_cookie(reservation_id);
        const auto &it = reservations.emplace(reservation_id, info).first;

        return Reservation(
            reservation_id,
            it->second.event_id,
            it->second.cookie,
            it->second.ticket_count,
            expiration_time
        );
    }

    void remove_reservation(const uint32_t reservation_id) {
        try {
            const auto &record = reservations.at(reservation_id);
            auto &event = events.at(record.event_id);
            event.ticket_count += record.ticket_count;
            reservations.erase(reservation_id);
        } catch (std::exception &e) {
            // ignore
        }
    }

    void clean_queue(const uint64_t current_time) {
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
};

