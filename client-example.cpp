#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>
#include<csignal>

#define MEM_SAMPLE_SERVICE_ID 0x1111
#define MEM_SAMPLE_INSTANCE_ID 0x2222
#define MEM_SAMPLE_METHOD_ID 0x0001

#define CPU_SAMPLE_SERVICE_ID 0x3333
#define CPU_SAMPLE_INSTANCE_ID 0x4444
#define CPU_SAMPLE_METHOD_ID 0x0002

struct MemInfo {
    long totalRAM;
    long freeRAM;
};

class ClientApp {
public:
    static std::shared_ptr<ClientApp> get_instance() {
        if (!instance) {
            instance = std::shared_ptr<ClientApp>(new ClientApp());
        }
        return instance;
    }
   
    void init();
    void start();
    void run_MEM();
    void run_CPU();
    void requestLoop();

    static void on_availability_MEM(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available);
    static void on_availability_CPU(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available);
    static void on_message_MEM(const std::shared_ptr<vsomeip::message> &_response);
    static void on_message_CPU(const std::shared_ptr<vsomeip::message> &_response);

private:
    static std::shared_ptr<ClientApp> instance;
    std::shared_ptr<vsomeip::application> app;
    std::mutex mem_mutex;
    std::mutex cpu_mutex;

    // Moved condition variables outside the class definition
    static std::condition_variable mem_condition;
    static std::condition_variable cpu_condition;

    std::atomic<bool> exitRequested{false};

    std::atomic<int> userChoice{-1};

    ClientApp() {}
};

std::shared_ptr<ClientApp> ClientApp::instance = nullptr;
std::condition_variable ClientApp::mem_condition;
std::condition_variable ClientApp::cpu_condition;

void ClientApp::init() {
    app = vsomeip::runtime::get()->create_application("Client");
    app->init();

    app->register_availability_handler(MEM_SAMPLE_SERVICE_ID, MEM_SAMPLE_INSTANCE_ID, on_availability_MEM);
    app->register_availability_handler(CPU_SAMPLE_SERVICE_ID, CPU_SAMPLE_INSTANCE_ID, on_availability_CPU);

    app->request_service(MEM_SAMPLE_SERVICE_ID, MEM_SAMPLE_INSTANCE_ID);
    app->request_service(CPU_SAMPLE_SERVICE_ID, CPU_SAMPLE_INSTANCE_ID);

    app->register_message_handler(MEM_SAMPLE_SERVICE_ID, MEM_SAMPLE_INSTANCE_ID, MEM_SAMPLE_METHOD_ID, on_message_MEM);
    app->register_message_handler(CPU_SAMPLE_SERVICE_ID, CPU_SAMPLE_INSTANCE_ID, CPU_SAMPLE_METHOD_ID, on_message_CPU);
}

void ClientApp::start() {
std::signal(SIGINT, SIG_DFL);

    app->start();
}

void ClientApp::run_MEM() {
 while(1){
    std::unique_lock<std::mutex> its_lock(mem_mutex);
    mem_condition.wait(its_lock);

    std::shared_ptr<vsomeip::message> request;
    request = vsomeip::runtime::get()->create_request();
    request->set_service(MEM_SAMPLE_SERVICE_ID);
    request->set_instance(MEM_SAMPLE_INSTANCE_ID);
    request->set_method(MEM_SAMPLE_METHOD_ID);

    app->send(request);

    // Release the lock, which will automatically reacquire it when exiting the scope
    its_lock.unlock();
}
}

void ClientApp::run_CPU() {
while(1){
    std::unique_lock<std::mutex> its_lock(cpu_mutex);
    cpu_condition.wait(its_lock);

    std::shared_ptr<vsomeip::message> request;
    request = vsomeip::runtime::get()->create_request();
    request->set_service(CPU_SAMPLE_SERVICE_ID);
    request->set_instance(CPU_SAMPLE_INSTANCE_ID);
    request->set_method(CPU_SAMPLE_METHOD_ID);
    
  if(  app->is_available(CPU_SAMPLE_SERVICE_ID,  CPU_SAMPLE_INSTANCE_ID))
            {
            std::cout<<"CPU service is available so request\n";
            }

    app->send(request);

    // Release the lock, which will automatically reacquire it when exiting the scope
    its_lock.unlock();
}
}

void ClientApp::requestLoop() {
    while (true) {
        std::cout << "Enter your choice:\n";
        std::cout << "1. Request Memory Info\n";
        std::cout << "2. Request CPU Info\n";
        std::cout << "3. Exit\n";

        int choice;
        std::cin >> choice;

        //userChoice.store(choice);

        switch (choice) {
            case 1:
                mem_condition.notify_one();
                break;
            case 2:
                cpu_condition.notify_one();
                break;
            default:
                std::cerr << "Invalid choice. Please enter a valid option.\n";
               
        }
    }
}

void ClientApp::on_availability_MEM(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
    std::cout << "CLIENT: Memory Usage Service ["
              << std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
              << "] is "
              << (_is_available ? "available." : "NOT available.")
              << std::endl;
}

void ClientApp::on_availability_CPU(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
    std::cout << "CLIENT: CPU Usage Service ["
              << std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
              << "] is "
              << (_is_available ? "available." : "NOT available.")
              << std::endl;
}

void ClientApp::on_message_MEM(const std::shared_ptr<vsomeip::message> &_response) {
    std::shared_ptr<vsomeip::payload> its_payload = _response->get_payload();
    vsomeip::length_t l = its_payload->get_length();

    if (l == sizeof(MemInfo)) {
        MemInfo Rcv_data;
        memcpy(&Rcv_data, its_payload->get_data(), sizeof(Rcv_data));

        std::cout << std::dec << "CLIENT: Received Memory information:\n"
                  << "Total RAM: " << Rcv_data.totalRAM << " KB\n"
                  << "Free RAM: " << Rcv_data.freeRAM << " KB\n";
    } else {
        std::cerr << "CLIENT: Error - Unexpected payload size.\n";
    }
}

void ClientApp::on_message_CPU(const std::shared_ptr<vsomeip::message> &_response) {
    std::shared_ptr<vsomeip::payload> its_payload = _response->get_payload();
    vsomeip::length_t l = its_payload->get_length();

    if (l == sizeof(double)) {
        double CPU_info;
        memcpy(&CPU_info, its_payload->get_data(), sizeof(CPU_info));

        std::cout << std::dec << "CLIENT: Received CPU information:\n"
                  << "CPU Usage of server : " << CPU_info << "%"
                  << std::endl;
    } else {
        std::cerr << "CLIENT: Error - Unexpected payload size.\n";
    }
}

int main() {
    auto client_app = ClientApp::get_instance();
    client_app->init();

    std::thread sender_mem(&ClientApp::run_MEM, client_app);
    std::thread sender_cpu(&ClientApp::run_CPU, client_app);
    std::thread request_loop(&ClientApp::requestLoop, client_app);

    client_app->start();

    sender_mem.join();
    sender_cpu.join();
    request_loop.join();

    return 0;
}

