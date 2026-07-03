#include <iostream>

#include "device/DeviceManager.h"
#include "filters/DefaultPipeFilters.h"
#include "aixlog.hpp"

int main() {
    using namespace std;
    using namespace hwlib;

    int ctrs = 0;

    auto dev = DeviceManager();
    //DriverConfig::parse("driver.xml");
    AixLog::Log::init<AixLog::SinkCout>(AixLog::Severity::trace);
    dev.setLogCallback([&](std::string msg) {
       LOG(INFO) << msg;
    });

    dev.initialize(AutoConfig().config());
    dev.setErrorCallback([&](auto f) {
        cout << "Error" << f.getMessages()[0] << endl;
    });


    dev.setContextCallback([&](const AssemblyContext &f) {
        ctrs++;
        //std::cout очень медленный и этот колбэк лучше использовать только при дебаге
        std::cout << "Got frame #" << ctrs << " Addr " << f.assemblyParams.id
                  << " Value is ["
                  << f.result->at(0) << ", " << f.result->at(1) << ", " << f.result->at(2) << " ... "
                  << f.result->at(f.result->size() - 3) << ", " << f.result->at(f.result->size() - 2)
                  << ", " << f.result->at(f.result->size() - 1) << "] len " << f.result->size() << std::endl;
        //delete[] f.result.data;
        //delete[] f.floatFrame.data;
        //delete[] f.rawFrame.data;
    });

    dev.connect();
    // Считываем интегрированый темновой сигнал
    dev.setPipeFilter(DefaultPipeFilter::instance(100));
    dev.setMeasurement(Measurement(Exposure(1000), 100, 0));
    std::cout << "Exposure: " << dev.getExposure().single << std::endl;
    auto darkSignal = dev.syncRead(100);

    // Асинхронно считываем 20 интегрированых кадров
    dev.setPipeFilter(DefaultPipeFilter::instance(100, darkSignal));
    dev.setMeasurement(Measurement(Exposure(1000), 2000, 0));

    dev.read();

    this_thread::sleep_for(chrono::seconds(20));
    dev.cancelReading();
    dev.disconnect();
}