//第17课3c节 配套示例程序
//http://www.mineserver.top


/*   * ----------------------------------------------------------------------------   
* "啤酒软件许可证" (第42次修改):   
* Stiven Ding 编写了这个文件。只要你保留这个公告，你就可以对这个软件做任何
* 你想做的事情。如果哪天我们碰到了，你认为这个软件还是有点价值的，可以请我喝杯
* 啤酒作为回报。
* ----------------------------------------------------------------------------   */

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Stiven Ding wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   <admin@sgsd.pw>
 * ----------------------------------------------------------------------------
 */


#include "pch.h"
#include "MainPage.xaml.h"

using namespace GpioOneWire;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Core;
using namespace Windows::System::Threading;
using namespace Windows::Devices::Gpio;


void GpioOneWire::DhtSensor::Init (GpioPin^ Pin)
{
    // 如果支持上拉则使用InputPullUp，反之使用Input模式
    this->inputDriveMode =
        Pin->IsDriveModeSupported(GpioPinDriveMode::InputPullUp) ?
        GpioPinDriveMode::InputPullUp : GpioPinDriveMode::Input;

    Pin->SetDriveMode(this->inputDriveMode);
    this->pin = Pin;
}

_Use_decl_annotations_
HRESULT GpioOneWire::DhtSensor::Sample (GpioOneWire::DhtSensorReading& Reading)
{
    Reading = DhtSensorReading();

    LARGE_INTEGER qpf;
    QueryPerformanceFrequency(&qpf);

	// 这是用于确定位是一个'0'或'1'的阈值。
	// 如果为“0”具有76微秒的脉冲的时间，而'1'具有120微秒的脉冲时间。
	// 所以，110被选择为一个合理的阈值。我们的值转换为QueryPerformanceFrequency(QPF)单位供以后使用。
	// QPF单位的相关信息：https://msdn.microsoft.com/zh-cn/library/windows/desktop/ms644905(v=vs.85).aspx
    const unsigned int oneThreshold = static_cast<unsigned int>(
        110LL * qpf.QuadPart / 1000000LL);

    // 下一步，我们将发送激活传感器所需的顺序。GPIO 信号
    // 通常被拉高，设备处于空闲状态，而我们必须把它拉低
    // 18毫秒请求的示例。我们闩向GPIO写入低电平。
    // 并将其设置为输出，。
	
    // 向针脚锁存低电平
    this->pin->Write(GpioPinValue::Low);

    // 将针脚设置为输出
    this->pin->SetDriveMode(GpioPinDriveMode::Output);

    // 等待至少18ms
    Sleep(SAMPLE_HOLD_LOW_MILLIS);

	// 我们然后还原到它走高，并等待输入 pin
    // DHT拉低，然后再高。
	
    // 将针脚重新设置回输入(或上拉输入)
    this->pin->SetDriveMode(this->inputDriveMode);

    GpioPinValue previousValue = this->pin->Read();

    // 捕捉第一个上升沿
    const ULONG initialRisingEdgeTimeoutMillis = 1;
    ULONGLONG endTickCount = GetTickCount64() + initialRisingEdgeTimeoutMillis;
    for (;;) {
        if (GetTickCount64() > endTickCount) {
            return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        }

        GpioPinValue value = this->pin->Read();
        if (value != previousValue) {
            // 若是上升沿
            if (value == GpioPinValue::High) {
                break;
            }
            previousValue = value;
        }
    }

    LARGE_INTEGER prevTime = { 0 };

    const ULONG sampleTimeoutMillis = 10;
    endTickCount = GetTickCount64() + sampleTimeoutMillis;

    // 收到的第一个上升沿后，我们捕捉所有的下降沿
    // 和测量时间的差值来确定是 0 或 1。

    // 捕捉每个下降沿，直到所有位都接收到或者超时错误
    for (unsigned int i = 0; i < (Reading.bits.size() + 1);) {
        if (GetTickCount64() > endTickCount) {
            return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        }

        GpioPinValue value = this->pin->Read();
        if ((previousValue == GpioPinValue::High) && (value == GpioPinValue::Low)) {
            // 检测到下降沿
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);

            if (i != 0) {
                unsigned int difference = static_cast<unsigned int>(
                    now.QuadPart - prevTime.QuadPart);
                Reading.bits[Reading.bits.size() - i] =
                    difference > oneThreshold;
            }

            prevTime = now;
            ++i;
        }

        previousValue = value;
    }

    if (!Reading.IsValid()) {
        // 校验错误
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }

    return S_OK;
}

MainPage::MainPage()
{
    InitializeComponent();
}

void GpioOneWire::MainPage::Page_Loaded(
    Platform::Object^ sender,
    Windows::UI::Xaml::RoutedEventArgs^ e
    )
{
    GpioController^ controller = GpioController::GetDefault();
    if (!controller) {
        this->statusText->Text = L"这个设备并没有GPIO控制器.";
        return;
    }

    GpioPin^ pin;
    try {
        pin = controller->OpenPin(DHT_PIN_NUMBER);
    } catch (Exception^ ex) {
        this->statusText->Text = L"打开GPIO失败：" + ex->Message;
        return;
    }

    this->dhtSensor.Init(pin);
    this->pullResistorText->Text = this->dhtSensor.PullResistorRequired() ?
        L"需要10kΩ上拉电阻." : L"不需要上拉电阻.";

    // 创建一个定时器每2秒从DHT采样
    TimeSpan period = { 2 * 10000000LL };
    this->timer = ThreadPoolTimer::CreatePeriodicTimer(
        ref new TimerElapsedHandler(this, &MainPage::timerElapsed),
        period);

    this->statusText->Text = L"状态：初始化成功.";
}

void GpioOneWire::MainPage::timerElapsed (
    Windows::System::Threading::ThreadPoolTimer^ Timer
    )
{
    HRESULT sensorHr;
    DhtSensorReading reading;

    int retryCount = 0;
    do {
        sensorHr = this->dhtSensor.Sample(reading);
    } while (FAILED(sensorHr) && (++retryCount < 20));

    String^ statusString;
    String^ humidityString;
    String^ temperatureString;

    if (FAILED(sensorHr)) {
        humidityString = L"湿度：(失败)";
        temperatureString = L"温度：(失败)";

        switch (sensorHr) {
        case __HRESULT_FROM_WIN32(ERROR_IO_DEVICE):
            statusString = L"无法捕捉所有沿.";
            break;
        case __HRESULT_FROM_WIN32(ERROR_TIMEOUT):
            statusString = L"等待采样超时.";
            break;
        case __HRESULT_FROM_WIN32(ERROR_INVALID_DATA):
            statusString = L"校验错误.";
            break;
        default:
            statusString = L"读取错误.";
        }
    }
    else
    {
        double humidity = reading.Humidity();
        double temperature = reading.Temperature();
        
        HRESULT hr;
        wchar_t buf[128];

        hr = StringCchPrintfW(
            buf,
            ARRAYSIZE(buf),
            L"湿度：%.1f%% RH",
            humidity);
        if (FAILED(hr)) {
            throw ref new Exception(hr, L"打印字符错误.");
        }

        humidityString = ref new String(buf);

        hr = StringCchPrintfW(
            buf,
            ARRAYSIZE(buf),
            L"温度：%.1f \u00B0C",
            temperature);
        if (FAILED(hr)) {
            throw ref new Exception(hr, L"打印字符错误.");
        }

        temperatureString = ref new String(buf);

        hr = StringCchPrintfW(
            buf,
            ARRAYSIZE(buf),
            L"成功 (%d%s)",
            retryCount,
            L" 次重试");
        if (FAILED(hr)) {
            throw ref new Exception(hr, L"打印字符错误.");
        }

        statusString = ref new String(buf);
    }

    this->Dispatcher->RunAsync(
        CoreDispatcherPriority::Normal,
        ref new DispatchedHandler([=] ()
    {
        this->statusText->Text = statusString;
        this->humidityText->Text = humidityString;
        this->temperatureText->Text = temperatureString;
    }));
}





void GpioOneWire::MainPage::button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	IsDHT22 = true;
}
