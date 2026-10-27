
#include <moodycamel/moodycamel.h>
#include <serial_port/serial_port.h>

#include "input_events.h"
#include "contracts/dcl/dcl_contracts.h"
#include "contracts/dcl/dcl_contracts_helper.h"


namespace Controls {
std::atomic<bool> shiftBtnState{false}, halfLevelState{false};
sigslot::signal<CPktMgr::HWMsg_t&> InputEvents::dispatchHardwareEvent = {};

class HwSerialData final : private juce::Thread {
public:
    HwSerialData() : Thread("KICK serial") {}
    ~HwSerialData() { close(); }
    void connect() {
        close();
        if (juce::SystemStats::getEnvironmentVariable("KICK_NO_SERIAL_DEVICE", {}) == "1" ||
            juce::SystemStats::getEnvironmentVariable("KICK_NO_HARDWARE", {}) == "1") return;
        auto path = juce::SystemStats::getEnvironmentVariable("KICK_SERIAL_DEVICE", {});
#if JUCE_LINUX && KICK_ENABLE_HARDWARE
        if (path.isEmpty()) path = "/dev/ttymxc0";
#endif
        if (path.isEmpty()) return;
        const auto result = port.open(path);
        if (result.failed()) { juce::Logger::writeToLog(result.getErrorMessage()); return; }
        CPktMgr::HWMsg_t stale; while (queue.try_dequeue(stale)) {}
        decoder.reset(); startThread();
        CPktMgr::HWMsg_t connected;
        connected.pktType = CPktMgr::EN_PKT_TYPE_SYSTEM;
        connected.pktCmd = contracts::DeviceControlContracts::EN_SYS_COMM_INIT;
        connected.data = {contracts::DeviceControlContracts::EN_SYS_COMM_CONNECTED};
        send(connected);
    }
    void close() { signalThreadShouldExit(); notify(); stopThread(1000); port.close(); }
    void send(const CPktMgr::HWMsg_t& message) {
        if (port.isOpen()) { queue.enqueue(message); notify(); }
    }
private:
    void run() override {
        std::array<uint8_t, 1024> bytes{};
        CPktMgr::HWMsg_t outgoing;
        while (!threadShouldExit()) {
            while (!threadShouldExit() && queue.try_dequeue(outgoing)) {
                auto encoded = kick::serial::encode({static_cast<uint8_t>(outgoing.pktType), static_cast<uint8_t>(outgoing.pktDir), static_cast<uint8_t>(outgoing.pktCmd), outgoing.data});
                if (!encoded.empty() && !port.write(encoded.data(), encoded.size())) { signalThreadShouldExit(); break; }
            }
            const auto count = port.read(bytes.data(), bytes.size());
            if (count < 0) break;
            if (count) decoder.feed(bytes.data(), static_cast<size_t>(count), [](const kick::serial::Message& incoming) {
                if (incoming.direction != CPktMgr::EN_PKT_REQUEST) return;
                CPktMgr::HWMsg_t message;
                message.pktType = static_cast<CPktMgr::enumPacketType>(incoming.type);
                message.pktDir = static_cast<CPktMgr::enumPacketDir>(incoming.direction);
                message.pktCmd = incoming.command; message.data = incoming.data;
                const auto required = message.pktType == CPktMgr::EN_PKT_TYPE_BUTTON ? 2 :
                    (message.pktType == CPktMgr::EN_PKT_TYPE_KNOBS || message.pktType == CPktMgr::EN_PKT_TYPE_PAD) ? 3 : 0;
                if (message.data.size() < static_cast<size_t>(required)) return;
                if (contracts::DclContractsHelper::isButtonMessage(message) && contracts::DclContractsHelper::getButtonCode(message) == contracts::DeviceControlContracts::EN_SHIFT)
                    shiftBtnState.store(contracts::DclContractsHelper::getButtonState(message));
                InputEvents::dispatchHardwareEvent(message);
            });
            if (!count) wait(2);
        }
        port.close();
    }
    kick::serial::Port port;
    kick::serial::Decoder decoder;
    moodycamel::ConcurrentQueue<CPktMgr::HWMsg_t> queue;
};
static std::unique_ptr<HwSerialData> frontPanel;
void InputEvents::initialize() { if (!frontPanel) frontPanel = std::make_unique<HwSerialData>(); frontPanel->connect(); }
void InputEvents::reconnect() { if (frontPanel) frontPanel->connect(); }
void InputEvents::shutdown() { frontPanel.reset(); }
void InputEvents::messageReceived(CPktMgr::HWMsg_t& message) { dispatchHardwareEvent(message); }
void InputEvents::sendMessageToHardware(CPktMgr::HWMsg_t& message) {
    if (message.pktType == CPktMgr::EN_PKT_TYPE_SYSTEM) {
        if (message.pktCmd == contracts::DeviceControlContracts::EN_SYS_RECONNECT) { reconnect(); return; }
        if (message.pktCmd == contracts::DeviceControlContracts::EN_SYS_SHUTDOWN) { shutdown(); return; }
    }
    if (frontPanel) frontPanel->send(message);
}
}
