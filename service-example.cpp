#include <iostream>
#include <fstream>
#include <sys/sysinfo.h>
#include <vsomeip/vsomeip.hpp>
#include <thread>
#include <sstream>

#define MEM_SAMPLE_SERVICE_ID 0x1111
#define MEM_SAMPLE_INSTANCE_ID 0x2222
#define MEM_SAMPLE_METHOD_ID 0x0001

#define CPU_SAMPLE_SERVICE_ID 0x3333
#define CPU_SAMPLE_INSTANCE_ID 0x4444
#define CPU_SAMPLE_METHOD_ID 0x0002


struct SysInfo {
    long total_RAM;
    long freeRAM;
};

struct CpuUsage {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
};

class Server {
public:
    Server() : app(vsomeip::runtime::get()->create_application("Server")) {}//So, in summary, this constructor initializes the app member of the Server class with a newly created application named "Server" using the vsomeip library.

    void init() {
        app->init();
        app->register_message_handler(MEM_SAMPLE_SERVICE_ID, MEM_SAMPLE_INSTANCE_ID, MEM_SAMPLE_METHOD_ID,
                                     [this](const std::shared_ptr<vsomeip::message>& request) {//In this case, the lambda takes a single parameter named request Inside the lambda function, it calls the member function onMessageMemoryUsage of the current object (this) with the provided request parameter.
                                         this->onMessageMemoryUsage(request);
                                     });

        app->register_message_handler(CPU_SAMPLE_SERVICE_ID, CPU_SAMPLE_INSTANCE_ID, CPU_SAMPLE_METHOD_ID,
                                     [this](const std::shared_ptr<vsomeip::message>& request) {
                                         this->onMessageCPUUsage(request);
                                     });

        app->offer_service(MEM_SAMPLE_SERVICE_ID, MEM_SAMPLE_INSTANCE_ID);
        app->offer_service(CPU_SAMPLE_SERVICE_ID, CPU_SAMPLE_INSTANCE_ID);
        app->start();
    }

  
private:
    std::shared_ptr<vsomeip::application> app;

    CpuUsage getCPUUsage() {
        CpuUsage usage = {0, 0, 0, 0};
        std::ifstream stat_file("/proc/stat");
        std::string line;

        std::istringstream iss;

        if (stat_file.is_open()) {
            std::getline(stat_file, line);
            iss.str(line);
            std::string cpu_label;

            if (iss >> cpu_label && cpu_label == "cpu") {
                iss >> usage.user >> usage.nice >> usage.system >> usage.idle;
            }
        }
        return usage;
    }

    void onMessageMemoryUsage(const std::shared_ptr<vsomeip::message>& request) {
        std::cout << "Client [" << request->get_client() << "] Requested for memory usage" << std::endl;

        std::shared_ptr<vsomeip::message> response = vsomeip::runtime::get()->create_response(request);
        struct sysinfo sysInfo;

        if (sysinfo(&sysInfo) != 0) {
            std::cerr << "Unable to Fetch Memory Usage" << std::endl;
            return;
        }

        SysInfo sysinfoData = {
            .total_RAM = static_cast<long>(sysInfo.totalram / 1024),
            .freeRAM = static_cast<long>(sysInfo.freeram / 1024),
        };

        std::shared_ptr<vsomeip::payload> payload = vsomeip::runtime::get()->create_payload();
        payload->set_data({reinterpret_cast<vsomeip::byte_t*>(&sysinfoData),
                           reinterpret_cast<vsomeip::byte_t*>(&sysinfoData) + sizeof(SysInfo)});

        response->set_payload(payload);
        app->send(response);

        std::cout << "Request Processed" << std::endl;
    }

    void onMessageCPUUsage(const std::shared_ptr<vsomeip::message>& request) {
        std::cout << "Client [" << request->get_client() << "] Requested for CPU usage" << std::endl;

        std::shared_ptr<vsomeip::message> response = vsomeip::runtime::get()->create_response(request);

        CpuUsage prevCPUUsage = getCPUUsage();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        CpuUsage currentCPUUsage = getCPUUsage();

        unsigned long long prevIdle = prevCPUUsage.idle + prevCPUUsage.nice;
        unsigned long long currentIdle = currentCPUUsage.idle + currentCPUUsage.nice;
        unsigned long long prevNonIdle = prevCPUUsage.user + prevCPUUsage.nice + prevCPUUsage.system;
        unsigned long long currentNonIdle = currentCPUUsage.user + currentCPUUsage.nice + currentCPUUsage.system;
        unsigned long long prevTotal = prevIdle + prevNonIdle;
        unsigned long long currentTotal = currentIdle + currentNonIdle;

        double cpuUsage = 100.0 * (1.0 - (currentIdle - prevIdle) / static_cast<double>(currentTotal - prevTotal));

        std::shared_ptr<vsomeip::payload> payload = vsomeip::runtime::get()->create_payload();
        payload->set_data({reinterpret_cast<vsomeip::byte_t*>(&cpuUsage),
                           reinterpret_cast<vsomeip::byte_t*>(&cpuUsage) + sizeof(cpuUsage)});

        response->set_payload(payload);
        app->send(response);

        std::cout << "Request Processed" << std::endl;
    }
};

int main() {
    Server server;
    server.init();
    
    // Sleep to keep the application running
    std::this_thread::sleep_for(std::chrono::hours(1));

    return 0;
}
