
#include <MQTTAsync.h>

#include <chrono>
#include <csics/io/net/MQTTEndpoint.hpp>
#include <thread>
#include <unordered_map>

#include <openssl/x509.h>
#include "csics/queue/queue.hpp"

namespace csics::io::net {

MQTTMessage::MQTTMessage() : payload_(), topic_(), internal_msg_(nullptr) {};
MQTTMessage::~MQTTMessage() {
    if (internal_msg_) {
        MQTTAsync_message* msg = static_cast<MQTTAsync_message*>(internal_msg_);
        MQTTAsync_freeMessage(&msg);
        MQTTAsync_free(const_cast<char*>(topic_.data()));
    };
};

MQTTMessage::MQTTMessage(StringView topic, BufferView payload)
    : payload_(payload), topic_(topic) {};

MQTTMessage::MQTTMessage(MQTTMessage&& other) noexcept
    : payload_(std::move(other.payload_)),
      topic_(other.topic_),
      internal_msg_(other.internal_msg_) {
    other.internal_msg_ = nullptr;
    other.topic_ = StringView();
    other.payload_ = BufferView();
};

MQTTMessage& MQTTMessage::operator=(MQTTMessage&& other) noexcept {
    if (this != &other) {
        payload_ = std::move(other.payload_);
        topic_ = other.topic_;
        internal_msg_ = other.internal_msg_;
        other.topic_ = StringView();
        other.payload_ = BufferView();
        other.internal_msg_ = nullptr;
    }
    return *this;
};

struct MQTTEndpoint::Internal {
    MQTTAsync client;
    MQTTEndpoint::MQTTConnectionParams params;
    std::unordered_map<std::string, queue::SPSCMessageQueue<MQTTMessage>>
        topic_queues;
    std::mutex topic_queues_mutex;
    using TimeStamp = std::chrono::time_point<std::chrono::steady_clock>;
    std::vector<std::tuple<TimeStamp, MQTTAsync_token, MQTTMessage>>
        pending_messages;
    String client_id;

    std::atomic<int> connected{0};

    ~Internal() {
        if (client) {
            MQTTAsync_disconnect(client, nullptr);
            MQTTAsync_destroy(&client);
        }
    }
};

void conn_lost_cb(void* ctx, char* cause) {
    MQTTEndpoint::conn_lost(ctx, cause);
};

int message_arrived_cb(void* context, char* topicName, int topicLen,
                       MQTTAsync_message* message) {
    return MQTTEndpoint::msg_arvd(context, topicName, topicLen, message);
};

void delivery_complete_cb(void* context, MQTTAsync_token token) {
    MQTTEndpoint::dlv_cmplt(context, token);
};

MQTTEndpoint::MQTTEndpoint(StringView client_id) : internal_(new Internal()) {
    internal_->client_id = String(client_id);
};
MQTTEndpoint::~MQTTEndpoint() { delete internal_; };

MQTTEndpoint::MQTTEndpoint(MQTTEndpoint&& other) noexcept
    : internal_(other.internal_) {
    other.internal_ = nullptr;
};

MQTTEndpoint& MQTTEndpoint::operator=(MQTTEndpoint&& other) noexcept {
    if (this != &other) {
        delete internal_;
        internal_ = other.internal_;
        other.internal_ = nullptr;
    }
    return *this;
};

NetStatus connect(MQTTEndpoint::Internal* internal_,
                  const MQTTEndpoint::MQTTConnectionParams& params) {
    MQTTAsync_create(&internal_->client, params.broker_uri.c_str(),
                     internal_->client_id.c_str(), MQTTCLIENT_PERSISTENCE_NONE,
                     nullptr);
    internal_->params = params;
    std::cerr << "Client ID: " << internal_->client_id << std::endl;
    std::cerr << "Broker URI: " << params.broker_uri.str() << std::endl;

    std::unique_ptr<MQTTAsync_SSLOptions> ssl_opts = nullptr;

    if (params.broker_uri.scheme() == "ssl" ||
        params.broker_uri.scheme() == "mqtts") {
        ssl_opts = std::make_unique<MQTTAsync_SSLOptions>();
        *ssl_opts = MQTTAsync_SSLOptions_initializer;
        // TODO: re-enable this once it actually works. Paho's SSL support
        // isn't working for right now.
        ssl_opts->enableServerCertAuth = 0;
    };


    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    conn_opts.keepAliveInterval = 10;
    conn_opts.cleansession = 0;
    conn_opts.ssl = ssl_opts.get();
    conn_opts.automaticReconnect = 1;
    String username = params.username;
    String password = params.password;
    conn_opts.username =
        username.empty() ? nullptr : username.c_str();
    conn_opts.password =
        password.empty() ? nullptr : password.c_str();
    conn_opts.context = reinterpret_cast<void*>(&internal_->connected);

    conn_opts.onSuccess = [](void* context, MQTTAsync_successData*) {
        static_cast<std::atomic<int>*>(context)->store(
            1, std::memory_order_release);
    };

    conn_opts.onFailure = [](void* context, MQTTAsync_failureData* fd) {
        static_cast<std::atomic<int>*>(context)->store(
            -1, std::memory_order_release);
        std::cerr << "Failed to connect to MQTT broker" << std::endl;
        std::cerr << "Error code: " << fd->code << std::endl;
        std::cerr << "Error message: " << (fd->message ? fd->message : "null")
                  << std::endl;
    };

    MQTTAsync_setCallbacks(internal_->client, static_cast<void*>(internal_),
                           &conn_lost_cb, &message_arrived_cb,
                           &delivery_complete_cb);

    std::cerr << "TrustStore: " << (ssl_opts ? ssl_opts->trustStore ? ssl_opts->trustStore : "null" : "null") << std::endl;
    std::cerr << "sslptr: " << (void*)(conn_opts.ssl) << std::endl;

    int err = MQTTASYNC_SUCCESS;
    if ((err = MQTTAsync_connect(internal_->client, &conn_opts)) !=
        MQTTASYNC_SUCCESS) {
        std::cerr << "Failed to start MQTT connection" << std::endl;
        std::cerr << "Error code: " << err << std::endl;
        std::cerr << "Error message: " << MQTTAsync_strerror(err) << std::endl;
        return NetStatus::Error;
    }

    auto now = std::chrono::steady_clock::now();
    while (internal_->connected.load(std::memory_order_acquire) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::steady_clock::now() - now > std::chrono::seconds(10)) {
            return NetStatus::Timeout;
        }
    }
    if (internal_->connected.load(std::memory_order_acquire) == -1) {
        std::cerr << "MQTT connection failed" << std::endl;
        return NetStatus::Error;
    }

    return NetStatus::Success;
}

NetStatus MQTTEndpoint::connect_(const MQTTConnectionParams& params) {
    return ::csics::io::net::connect(internal_, params);
}

void MQTTEndpoint::conn_lost(void* ctx, char*) {
    // for now just try to reconnect immediately, but we might want to add some
    // backoff or something
    auto *internal = static_cast<MQTTEndpoint::Internal*>(ctx);
    if (MQTTAsync_reconnect(static_cast<MQTTAsync*>(internal->client) ) != MQTTASYNC_SUCCESS) {
        throw std::runtime_error("Failed to reconnect to MQTT broker");
    }
}

void MQTTEndpoint::dlv_cmplt(void* context, int token) {
    auto* internal = static_cast<MQTTEndpoint::Internal*>(context);
    std::erase_if(internal->pending_messages, [token](const auto& entry) {
        return std::get<1>(entry) == token;
    });
}

int MQTTEndpoint::msg_arvd(void* context, char* topicName, int topicLen,
                           void* message_) {
    auto* internal = static_cast<MQTTEndpoint::Internal*>(context);
    CSICS_RUNTIME_ASSERT(topicName != nullptr,
                         "Received message with null topic");
    CSICS_RUNTIME_ASSERT(message_ != nullptr, "Received null message");
    CSICS_RUNTIME_ASSERT(internal != nullptr,
                         "MQTT message arrived with null context");

    topicLen = topicLen > 0 ? topicLen : std::strlen(topicName) - 1;
    auto key = std::string(topicName, topicLen);

    std::lock_guard<std::mutex> lock(internal->topic_queues_mutex);

    auto q = internal->topic_queues.find(key);
    if (q == internal->topic_queues.end()) {
        // Not subscribed to this topic, ignore the message
        return 0;  // Indicate that the message was processed (ignored)
    }

    MQTTMessage msg{};
    msg.internal_msg_ = message_;
    msg.topic_ = StringView(topicName, topicLen);
    msg.payload_ =
        BufferView(static_cast<const char*>(
                       static_cast<MQTTAsync_message*>(message_)->payload),
                   static_cast<MQTTAsync_message*>(message_)->payloadlen);
    auto ret = q->second.try_push(std::move(msg));

    if (ret != queue::SPSCError::None) {
        // TODO: handle queue overflow, e.g., by dropping the message or logging
        // an error
        return 0;  // Indicate failure to process the message
    }

    return 1;
}

NetResult MQTTEndpoint::publish(MQTTMessage&& message) {
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

    int err = MQTTAsync_send(internal_->client, message.topic().data(),
                             message.payload().size(), message.payload().data(),
                             message.qos_, message.retained_, &opts);
    if (err != MQTTASYNC_SUCCESS) {
        return {NetStatus::Error, 0};
    }

    auto size = message.payload().size();

    internal_->pending_messages.emplace_back(std::chrono::steady_clock::now(),
                                             opts.token, std::move(message));

    return {NetStatus::Success, size};
};

NetStatus MQTTEndpoint::subscribe(const StringView topic, int qos) {
    auto key = std::string(topic.data(), topic.size());
    std::lock_guard<std::mutex> lock(internal_->topic_queues_mutex);
    internal_->topic_queues.emplace(key, 1024);

    int err =
        MQTTAsync_subscribe(internal_->client, topic.data(), qos, nullptr);
    if (err != MQTTASYNC_SUCCESS) {
        return NetStatus::Error;
    }
    return NetStatus::Success;
}

NetStatus MQTTEndpoint::recv(const StringView topic, MQTTMessage& message) {
    std::lock_guard<std::mutex> lock(internal_->topic_queues_mutex);
    auto queue =
        internal_->topic_queues.find(std::string(topic.data(), topic.size()));
    if (queue == internal_->topic_queues.end()) {
        return NetStatus::Error;  // Not subscribed to this topic
    }

    if (queue->second.empty()) {
        return NetStatus::Empty;
    }

    MQTTMessage msg{};
    auto ret = queue->second.try_pop(msg);
    if (ret == queue::SPSCError::Empty) {
        return NetStatus::Empty;
    } else if (ret != queue::SPSCError::None) {
        return NetStatus::Error;
    }
    message = std::move(msg);
    return NetStatus::Success;
};

PollStatus MQTTEndpoint::poll(const StringView topic, int timeoutMs) {
    {
        std::lock_guard<std::mutex> lock(internal_->topic_queues_mutex);
        auto queue = internal_->topic_queues.find(
            std::string(topic.data(), topic.size()));
        if (queue == internal_->topic_queues.end()) {
            return PollStatus::Error;  // Not subscribed to this topic
        }
    }

    auto key = std::string(topic.data(), topic.size());
    // simple check if the queue is not empty
    auto now = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - now <
           std::chrono::milliseconds(timeoutMs)) {
        {
            std::lock_guard<std::mutex> lock(internal_->topic_queues_mutex);

            auto queue_ref = internal_->topic_queues.find(key);

            if (queue_ref != internal_->topic_queues.end() &&
                !queue_ref->second.empty()) {
                return PollStatus::Ready;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };

    return PollStatus::Timeout;
}

};  // namespace csics::io::net
