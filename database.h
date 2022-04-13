#include <iostream>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <queue>
#include <optional>
#include <chrono>
#include <exception>
#include <cstring> // memcpy

constexpr uint64_t DEFAULT_TIMEOUT = 5;

constexpr int COOKIE_LEN = 48;
constexpr int MAX_DESCRIPTION_LEN = 80;
constexpr int TICKET_LEN = 7;

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
    Ticket *tickets;
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

        ReservationInfo() = default;
        ~ReservationInfo() = default;
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
    std::unordered_map<uint32_t, Event> events;
    std::unordered_map<uint32_t, ReservationInfo> reservations;
    std::queue<ReservationTime> reservation_queue;

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
                std::cerr << "The number of tickets available for the event of ID equal to " << event_id << " is too small.\n";
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

private:
    static uint64_t get_seconds_from_epoch() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }

    Reservation make_reservation(const uint32_t event_id, const uint16_t ticket_count, const uint64_t expiration_time) {
        uint32_t reservation_id = /* get it somehow lol */ 2;
        reservation_queue.push(ReservationTime(reservation_id, expiration_time));
        const auto &it = reservations.emplace(reservation_id, ReservationInfo()).first;

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

