#pragma once

#include "MainPage.g.h"

namespace GpioOneWire
{
	static bool IsDHT22;
    struct DhtSensorReading {
        bool IsValid ( ) const
        {
            unsigned long long value = this->bits.to_ullong();
            unsigned int checksum =
                ((value >> 32) & 0xff) +
                ((value >> 24) & 0xff) +
                ((value >> 16) & 0xff) +
                ((value >> 8) & 0xff);

            return (checksum & 0xff) == (value & 0xff);
        }


        double Humidity ( ) const
        {
            unsigned long long value = this->bits.to_ullong();
			return (IsDHT22 ? ((value >> 24) & 0xffff) * 0.1 : (value >> 32) & 0xff) + ((value >> 24) & 0xff) *0.1;
        }

        double Temperature ( ) const
        {
            unsigned long long value = this->bits.to_ullong();
			double temp = IsDHT22 ? ((value >> 8) & 0x7FFF) * 0.1 : ((value >> 16) & 0xff) + ((value >> 8) & 0xff);
            if ((value >> 8) & 0x8000)
                temp = -temp;
            return temp;
        }

        std::bitset<40> bits;
    };

    class DhtSensor
    {
        enum { SAMPLE_HOLD_LOW_MILLIS = 18 };

    public:

        DhtSensor ( ) :
            pin(nullptr),
            inputDriveMode(Windows::Devices::Gpio::GpioPinDriveMode::Input)
        { }

        void Init (Windows::Devices::Gpio::GpioPin^ Pin);

        HRESULT Sample (_Out_ DhtSensorReading& Reading);

        bool PullResistorRequired ( ) const
        {
            return inputDriveMode != Windows::Devices::Gpio::GpioPinDriveMode::InputPullUp;
        }

    private:
        Windows::Devices::Gpio::GpioPin^ pin;
        Windows::Devices::Gpio::GpioPinDriveMode inputDriveMode;
    };

    /// <summary>
    /// The main page of the application - used to show samples from the DHT Sensor.
    /// </summary>
    public ref class MainPage sealed
    {
        enum { DHT_PIN_NUMBER = 4 }; 


    public:
        MainPage();

    private:
        void Page_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void timerElapsed (Windows::System::Threading::ThreadPoolTimer^ Timer);

        DhtSensor dhtSensor;
        Windows::System::Threading::ThreadPoolTimer^ timer;
		void button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	};
}
