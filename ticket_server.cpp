#include "database.h"

#include <filesystem>
#include <fstream>

#include <iostream>
#include <cassert>
#include <cstring> // memcpy


///////////////////////////
///                     ///
///     CONSTANTS       ///
///                     ///
///////////////////////////


constexpr uint8_t EVENTS_ID = 1;
constexpr uint8_t EVENTS_REPLY_ID = 2;
constexpr uint8_t RESERVATION_ID = 3;
constexpr uint8_t RESERVATION_REPLY_ID = 4;
constexpr uint8_t TICKETS_ID = 5;
constexpr uint8_t TICKETS_REPLY_ID = 6;
constexpr uint8_t BAD_REQUEST_ID = 255;

constexpr size_t MAX_CONTENT_SIZE = 65507;
constexpr size_t MAX_REQUEST_SIZE = 53;


///////////////////////
///                 ///
///     LOADING     ///
///      DATA       ///
///                 ///
///////////////////////


struct ServerParameters {
    std::string filepath;
    uint16_t port;
    uint32_t timeout;
};

ServerParameters parse_parameters(int argc, char *argv[]) {
    return ServerParameters(); // TODO
}

Database load_database(const ServerParameters &parameters) {
    Database result(parameters.timeout);

    std::ifstream file;
    file.open(parameters.filepath);
    std::string description;
    std::string number;

    // Assumes the content of the file is correct
    while (std::getline(file, description)) {
        if (!std::getline(file, number))
            break;
        result.add_event(description, std::stoi(number));
    }

    return result;
}


///////////////////////
///                 ///
///    MESSAGES     ///
///                 ///
///////////////////////


struct Message {
    uint8_t message_id;

    Message() = delete;
    Message(uint8_t id)
    : message_id(id) {}
    ~Message() = default;
};

struct ReservationMessage : public Message {
    Reservation data;

    ReservationMessage() = delete;
    ReservationMessage(const Reservation &reservation)
    : Message(RESERVATION_REPLY_ID)
    , data(reservation) {}

    ReservationMessage(Reservation &&reservation)
    : Message(RESERVATION_REPLY_ID)
    , data(std::move(reservation)) {}
};

struct BadRequest : public Message {
    uint32_t request_id;

    BadRequest() = delete;
    BadRequest(uint32_t request_id_)
    : Message(BAD_REQUEST_ID)
    , request_id(request_id_) {}
    ~BadRequest() = default;
};

template<typename T>
void save_to_buffer(char *buffer, T number) {
    // char* -> unsigned char*?
    memcpy(buffer, reinterpret_cast<char*>(&number), sizeof(T));
}

struct MessageWrapper {
    char *buffer = nullptr;
    size_t current_byte;

    MessageWrapper() = delete;
    MessageWrapper(size_t buffer_size) {
        assert(buffer_size);
        buffer = new char[buffer_size];
    }
    virtual ~MessageWrapper() {
        if (buffer)
            delete [] buffer;
    }
};

struct EventMessageWrapper : public MessageWrapper {
    EventMessageWrapper() = delete;

    EventMessageWrapper(size_t buffer_size)
    : MessageWrapper(buffer_size)
    {
        buffer[0] = EVENTS_REPLY_ID;
        current_byte = 1;
    }

    ~EventMessageWrapper() = default;

    void add_event(const Event &event) {
        save_to_buffer(&buffer[current_byte], event.event_id);
        current_byte += sizeof(event.event_id);
        save_to_buffer(&buffer[current_byte], event.ticket_count);
        current_byte += sizeof(event.ticket_count);
        save_to_buffer(&buffer[current_byte], static_cast<uint8_t>(event.description.length()));
        current_byte += sizeof(uint8_t);
        event.description.copy(&buffer[current_byte], event.description.length());
        current_byte += event.description.length();
    }
};

inline size_t get_event_size(const Event &event) {
    return sizeof(event.event_id)
            + sizeof(event.ticket_count)
            + sizeof(uint8_t)
            + event.description.length();
}

void handle_events(const Database &db) {
    size_t size = sizeof(Message);
    size_t event_count = 0;

    for (auto it = db.events_begin(); it != db.events_end(); ++it) {
        const size_t event_size = get_event_size(*it);
        if (size + event_size <= MAX_CONTENT_SIZE)
            size += event_size;
        else
            break;
        ++event_count;
    }

    EventMessageWrapper wrapper(size);
    for (auto it = db.events_begin(); it != db.events_end(); ++it)
        wrapper.add_event(*it);
    // send(wrapper.buffer); // TODO
}


struct ReservationRequest {
    uint32_t event_id;
    uint16_t ticket_count;

    ReservationRequest(uint32_t event_id_, uint16_t ticket_count_)
    : event_id(event_id_)
    , ticket_count(ticket_count_) {}
};

inline ReservationRequest cast_reservation_request(char const *request) {
    uint32_t event_id = *reinterpret_cast<const uint32_t*>(/* ntohs */ request);
    uint16_t ticket_count = *reinterpret_cast<const uint16_t*>(/* ntohs */ request + sizeof(uint32_t));
    return ReservationRequest(event_id, ticket_count);
}

void handle_reservation(Database &db, char const *request) {
    ReservationRequest request_info = cast_reservation_request(request);
    try {
        ReservationMessage message(db.make_reservation(request_info.event_id, request_info.ticket_count));
        // send(message);
    } catch (std::exception &e) {
        std::cerr << e.what() << "\n";
        BadRequest error(request_info.event_id);
        // send(error);
    }
}



struct TicketsRequest {
    uint32_t reservation_id;
    char const *cookie;

    TicketsRequest(uint32_t reservation_id_, char const *cookie_)
    : reservation_id(reservation_id_)
    , cookie(cookie_) {}
};

inline TicketsRequest cast_tickets_request(char const *request) {
    uint32_t reservation_id = *reinterpret_cast<const uint32_t*>(/* ntohs */ request);
    char const *cookie = request + sizeof(uint32_t);
    return TicketsRequest(reservation_id, cookie);
}

void handle_tickets(Database &db, char const *request) {
    TicketsRequest tickets_info = cast_tickets_request(request);
    try {
        auto tickets = db.get_tickets(tickets_info.reservation_id, tickets_info.cookie);

    } catch (std::exception &e) {
        std::cerr << e.what() << "\n";
        BadRequest error(tickets_info.reservation_id);
        // send(error);
    }
}

void run(const ServerParameters &parameters) {
    Database db = load_database(parameters);
}

int main(int argc, char *argv[]) {
    // we don't need the program's name
    auto server_parameters = parse_parameters(argc - 1, &argv[1]);
    run(server_parameters);

    return 0;
}
