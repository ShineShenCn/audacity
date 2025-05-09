/*
* Audacity: A Digital Audio Editor
*/

#include "internal/wxtypes_convert.h"
#include "au3wrap/au3types.h"

#include "libraries/lib-audio-devices/DeviceManager.h"
#include "libraries/lib-audio-devices/AudioIOBase.h"
#include "libraries/lib-audio-io/AudioIO.h"
#include "libraries/lib-utility/IteratorX.h"
#include "QualitySettings.h"
#include "ProjectRate.h"

#include "log.h"
#include "realfn.h"
#include <portaudio.h>

#include "au3audiodevicesprovider.h"

using namespace muse;
using namespace au::au3;

void Au3AudioDevicesProvider::init()
{
    initHosts();
    initHostDevices();
    initInputChannels();
}

std::vector<std::string> Au3AudioDevicesProvider::audioOutputDevices() const
{
    std::vector<std::string> outputDevices;
    const std::vector<DeviceSourceMap>& outMaps = DeviceManager::Instance()->GetOutputDeviceMaps();
    auto host = AudioIOHost.Read();

    for (const auto& device : outMaps) {
        if (device.hostString == host) {
            outputDevices.push_back(wxToStdSting(MakeDeviceSourceString(&device)));
        }
    }

    return outputDevices;
}

std::string Au3AudioDevicesProvider::currentAudioOutputDevice() const
{
    return wxToStdSting(AudioIOPlaybackDevice.Read());
}

void Au3AudioDevicesProvider::setAudioOutputDevice(const std::string& deviceName)
{
    AudioIOPlaybackDevice.Write(wxString::FromUTF8(deviceName));

    Au3AudioDevicesProvider::handleDeviceChange();

    m_audioOutputDeviceChanged.notify();
}

async::Notification Au3AudioDevicesProvider::audioOutputDeviceChanged() const
{
    return m_audioOutputDeviceChanged;
}

std::vector<std::string> Au3AudioDevicesProvider::audioInputDevices() const
{
    std::vector<std::string> inputDevices;
    const std::vector<DeviceSourceMap>& inMaps = DeviceManager::Instance()->GetInputDeviceMaps();
    auto host = AudioIOHost.Read();

    for (const auto& device : inMaps) {
        if (device.hostString == host) {
            inputDevices.push_back(wxToStdSting(MakeDeviceSourceString(&device)));
        }
    }

    return inputDevices;
}

std::string Au3AudioDevicesProvider::currentAudioInputDevice() const
{
    return wxToStdSting(AudioIORecordingDevice.Read());
}

void Au3AudioDevicesProvider::setAudioInputDevice(const std::string& deviceName)
{
    std::vector<std::string> inputDevices;
    const std::vector<DeviceSourceMap>& inMaps = DeviceManager::Instance()->GetInputDeviceMaps();
    auto host = AudioIOHost.Read();

    long newChannels = 0;
    auto oldChannels = AudioIORecordChannels.Read();

    wxArrayStringEx names;
    for (const auto& device : inMaps) {
        if (device.hostString == host && wxToStdSting(MakeDeviceSourceString(&device)) == deviceName) {
            AudioIORecordingDevice.Write(wxString::FromUTF8(deviceName));
            AudioIORecordingSourceIndex.Write(device.sourceIndex);
            if (device.totalSources >= 1) {
                AudioIORecordingSource.Write(device.sourceString);
            } else {
                AudioIORecordingSource.Reset();
            }

            for (size_t j = 0; j < (unsigned int)device.numChannels; j++) {
                wxString name;

                if (j == 0) {
                    name = _("1 (Mono) Recording Channel");
                } else if (j == 1) {
                    name = _("2 (Stereo) Recording Channels");
                } else {
                    name = wxString::Format(wxT("%d"), (int)j + 1);
                }
                names.push_back(name);
            }
            newChannels = device.numChannels;
            if (oldChannels <= newChannels && oldChannels >= 1) {
                newChannels = oldChannels;
            }
            AudioIORecordChannels.Write(newChannels);

            Au3AudioDevicesProvider::handleDeviceChange();
        }
    }

    mInputChannels.Set(std::move(names));
    if (newChannels >= 1) {
        // Correct to 0-based index in choice
        mInputChannels.Set(newChannels - 1);
    }

    m_audioInputDeviceChanged.notify();
    m_inputChannelsListChanged.notify();
    m_inputChannelsChanged.notify();
}

async::Notification Au3AudioDevicesProvider::audioInputDeviceChanged() const
{
    return m_audioInputDeviceChanged;
}

void Au3AudioDevicesProvider::handleDeviceChange()
{
    AudioIO::Get()->HandleDeviceChange();
}

std::vector<std::string> Au3AudioDevicesProvider::audioApiList() const
{
    const std::vector<DeviceSourceMap>& outMaps = DeviceManager::Instance()->GetOutputDeviceMaps();
    std::vector<std::string> hosts;

    for (const auto& device : outMaps) {
        std::string host = wxToStdSting(device.hostString);
        if (std::find(hosts.begin(), hosts.end(), host) == hosts.end()) {
            hosts.push_back(host);
        }
    }
    return hosts;
}

std::string Au3AudioDevicesProvider::currentAudioApi() const
{
    const std::vector<DeviceSourceMap>& outMaps = DeviceManager::Instance()->GetOutputDeviceMaps();
    auto host = AudioIOHost.Read();

    for (const auto& device : outMaps) {
        if (device.hostString == host) {
            return wxToStdSting(device.hostString);
        }
    }
    return std::string();
}

void Au3AudioDevicesProvider::setAudioApi(const std::string& audioApi)
{
    const std::vector<DeviceSourceMap>& outMaps = DeviceManager::Instance()->GetOutputDeviceMaps();

    for (const auto& device : outMaps) {
        if (device.hostString == wxString(audioApi)) {
            AudioIOHost.Write(device.hostString);
            m_audioApiChanged.notify();

            updateInputOutputDevices();

            return;
        }
    }
}

std::vector<std::string> Au3AudioDevicesProvider::inputChannelsList() const
{
    const std::vector<DeviceSourceMap>& inMaps = DeviceManager::Instance()->GetInputDeviceMaps();
    auto host = AudioIOHost.Read();
    auto device = AudioIORecordingDevice.Read();
    auto source = AudioIORecordingSource.Read();
    long newChannels = 0;

    auto oldChannels = AudioIORecordChannels.Read();

    std::vector<std::string> names;
    for (auto& dev: inMaps) {
        if (source == dev.sourceString
            && device == dev.deviceString
            && host == dev.hostString) {
            // add one selection for each channel of this source
            for (size_t j = 0; j < (unsigned int)dev.numChannels; j++) {
                wxString name;

                if (j == 0) {
                    name = _("1 (Mono) Recording Channel");
                } else if (j == 1) {
                    name = _("2 (Stereo) Recording Channels");
                } else {
                    name = wxString::Format(wxT("%d"), (int)j + 1);
                }
                names.push_back(name.ToStdString());
            }
        }
    }

    return names;
}

std::string Au3AudioDevicesProvider::currentInputChannels() const
{
    int currentRecordChannels = AudioIORecordChannels.Read();

    if (inputChannelsList().empty()) {
        return std::string();
    }

    wxString name;
    if (currentRecordChannels == 1) {
        name = _("1 (Mono) Recording Channel");
    } else if (currentRecordChannels == 2) {
        name = _("2 (Stereo) Recording Channels");
    } else {
        name = wxString::Format(wxT("%d"), currentRecordChannels);
    }

    return name.ToStdString();
}

void Au3AudioDevicesProvider::setInputChannels(const std::string& newChannels)
{
    std::optional<int> channelsToWrite;
    for (const auto& channels : inputChannelsList()) {
        if (channels == newChannels) {
            if (channels == _("1 (Mono) Recording Channel")) {
                channelsToWrite = 1;
            } else if (channels == _("2 (Stereo) Recording Channels")) {
                channelsToWrite = 2;
            } else {
                channelsToWrite = std::stoi(channels);
            }
            break;
        }
    }
    if (channelsToWrite.has_value()) {
        AudioIORecordChannels.Write(channelsToWrite.value());
        m_inputChannelsChanged.notify();
    }
}

double Au3AudioDevicesProvider::bufferLength() const
{
    return AudioIOLatencyDuration.Read();
}

void Au3AudioDevicesProvider::setBufferLength(double newBufferLength)
{
    if (!muse::RealIsEqualOrMore(newBufferLength, 0.0)) {
        AudioIOLatencyDuration.Reset();
        m_bufferLengthChanged.notify();
        return;
    }

    AudioIOLatencyDuration.Write(newBufferLength);
    m_bufferLengthChanged.notify();
}

double Au3AudioDevicesProvider::latencyCompensation() const
{
    return AudioIOLatencyCorrection.Read();
}

void Au3AudioDevicesProvider::setLatencyCompensation(double newLatencyCompensation)
{
    AudioIOLatencyCorrection.Write(newLatencyCompensation);
    m_latencyCompensationChanged.notify();
}

std::vector<uint64_t> Au3AudioDevicesProvider::availableSampleRateList() const
{
    std::vector<uint64_t> rates;
    for (int i = 0; i < AudioIOBase::NumStandardRates; ++i) {
        int iRate = AudioIOBase::StandardRates[i];
        rates.push_back(iRate);
    }

    return rates;
}

uint64_t Au3AudioDevicesProvider::defaultSampleRate() const
{
    int intRate = 0;
    QualitySettings::DefaultSampleRate.Read(&intRate);
    return intRate;
}

void Au3AudioDevicesProvider::setDefaultSampleRate(uint64_t newRate)
{
    QualitySettings::DefaultSampleRate.Write(static_cast<int>(newRate));
    auto currentProject = globalContext()->currentProject();
    if (currentProject) {
        Au3Project* project = reinterpret_cast<Au3Project*>(currentProject->au3ProjectPtr());
        ::ProjectRate::Get(*project).SetRate(newRate);
    }

    m_defaultSampleRateChanged.notify();
}

void Au3AudioDevicesProvider::setDefaultSampleFormat(const std::string& format)
{
    for (const auto& symbol : QualitySettings::SampleFormatSetting.GetSymbols()) {
        if (format == symbol.Msgid().Translation()) {
            QualitySettings::SampleFormatSetting.Write(wxString(symbol.Internal()));
            m_defaultSampleFormatChanged.notify();
        }
    }
}

async::Notification Au3AudioDevicesProvider::defaultSampleFormatChanged() const
{
    return m_defaultSampleFormatChanged;
}

std::string Au3AudioDevicesProvider::defaultSampleFormat() const
{
    auto currentFormat = QualitySettings::SampleFormatSetting.Read();
    for (const auto& symbol : QualitySettings::SampleFormatSetting.GetSymbols()) {
        if (currentFormat == symbol.Internal()) {
            return symbol.Msgid().Translation().ToStdString();
        }
    }
}

std::vector<std::string> Au3AudioDevicesProvider::defaultSampleFormatList() const
{
    std::vector<std::string> sampleFormatList;
    for (const auto& format : QualitySettings::SampleFormatSetting.GetSymbols().GetMsgids()) {
        sampleFormatList.push_back(format.Translation().ToStdString());
    }

    return sampleFormatList;
}

async::Notification Au3AudioDevicesProvider::defaultSampleRateChanged() const
{
    return m_defaultSampleRateChanged;
}

async::Notification Au3AudioDevicesProvider::latencyCompensationChanged() const
{
    return m_latencyCompensationChanged;
}

async::Notification Au3AudioDevicesProvider::bufferLengthChanged() const
{
    return m_bufferLengthChanged;
}

async::Notification Au3AudioDevicesProvider::inputChannelsListChanged() const
{
    return m_inputChannelsListChanged;
}

async::Notification Au3AudioDevicesProvider::inputChannelsChanged() const
{
    return m_inputChannelsChanged;
}

async::Notification Au3AudioDevicesProvider::audioApiChanged() const
{
    return m_audioApiChanged;
}

void Au3AudioDevicesProvider::initHosts()
{
    const std::vector<DeviceSourceMap>& inMaps = DeviceManager::Instance()->GetInputDeviceMaps();
    const std::vector<DeviceSourceMap>& outMaps = DeviceManager::Instance()->GetOutputDeviceMaps();

    wxArrayString hosts;

    // go over our lists add the host to the list if it isn't there yet

    for (auto& device : inMaps) {
        if (!make_iterator_range(hosts).contains(device.hostString)) {
            hosts.push_back(device.hostString);
        }
    }

    for (auto& device : outMaps) {
        if (!make_iterator_range(hosts).contains(device.hostString)) {
            hosts.push_back(device.hostString);
        }
    }

    mHost.Set(std::move(hosts));
}

void Au3AudioDevicesProvider::initHostDevices()
{
    const std::vector<DeviceSourceMap>& inMaps = DeviceManager::Instance()->GetInputDeviceMaps();
    const std::vector<DeviceSourceMap>& outMaps = DeviceManager::Instance()->GetOutputDeviceMaps();

    //read what is in the prefs
    auto host = AudioIOHost.Read();
    int foundHostIndex = -1;

    // if the host is not in the hosts combo then we rescanned.
    // set it to blank so we search for another host.
    if (mHost.Find(host) < 0) {
        host = wxT("");
    }

    // Try to find a hostIndex, among either inputs or outputs, assumed to be
    // unique among the union of the set of input and output devices
    for (auto& device : outMaps) {
        if (device.hostString == host) {
            foundHostIndex = device.hostIndex;
            break;
        }
    }

    if (foundHostIndex == -1) {
        for (auto& device : inMaps) {
            if (device.hostString == host) {
                foundHostIndex = device.hostIndex;
                break;
            }
        }
    }

    // If no host was found based on the prefs device host, load the first available one
    if (foundHostIndex == -1) {
        if (outMaps.size()) {
            foundHostIndex = outMaps[0].hostIndex;
        } else if (inMaps.size()) {
            foundHostIndex = inMaps[0].hostIndex;
        }
    }

    // Make sure in/out are clear in case no host was found
    mInput.Clear();
    mOutput.Clear();

    // If we still have no host it means no devices, in which case do nothing.
    if (foundHostIndex == -1) {
        return;
    }

    // Repopulate the Input/Output device list available to the user
    wxArrayStringEx mInputDeviceNames;
    for (size_t i = 0; i < inMaps.size(); ++i) {
        auto& device = inMaps[i];
        if (foundHostIndex == device.hostIndex) {
            mInputDeviceNames.push_back(MakeDeviceSourceString(&device));
            if (host.empty()) {
                host = device.hostString;
                AudioIOHost.Write(host);
                mHost.Set(host);
            }
        }
    }
    mInput.Set(std::move(mInputDeviceNames));

    wxArrayStringEx mOutputDeviceNames;
    for (size_t i = 0; i < outMaps.size(); ++i) {
        auto& device = outMaps[i];
        if (foundHostIndex == device.hostIndex) {
            mOutputDeviceNames.push_back(MakeDeviceSourceString(&device));
            if (host.empty()) {
                host = device.hostString;
                AudioIOHost.Write(host);
                mHost.Set(host);
            }
        }
    }
    mOutput.Set(std::move(mOutputDeviceNames));

    gPrefs->Flush();

    // The setting of the Device is left up to menu handlers
}

void Au3AudioDevicesProvider::initInputChannels()
{
    //!NOTE: Copied from AudioSetupToolBar::FillInputChannels

    const std::vector<DeviceSourceMap>& inMaps = DeviceManager::Instance()->GetInputDeviceMaps();
    auto host = AudioIOHost.Read();
    auto device = AudioIORecordingDevice.Read();
    auto source = AudioIORecordingSource.Read();
    long newChannels = 0;

    auto oldChannels = AudioIORecordChannels.Read();

    wxArrayStringEx names;
    for (auto& dev: inMaps) {
        if (source == dev.sourceString
            && device == dev.deviceString
            && host == dev.hostString) {
            // add one selection for each channel of this source
            for (size_t j = 0; j < (unsigned int)dev.numChannels; j++) {
                wxString name;

                if (j == 0) {
                    name = _("1 (Mono) Recording Channel");
                } else if (j == 1) {
                    name = _("2 (Stereo) Recording Channels");
                } else {
                    name = wxString::Format(wxT("%d"), (int)j + 1);
                }
                names.push_back(name);
            }
            newChannels = dev.numChannels;
            if (oldChannels <= newChannels && oldChannels >= 1) {
                newChannels = oldChannels;
            }
            AudioIORecordChannels.Write(newChannels);
            break;
        }
    }
    mInputChannels.Set(std::move(names));
    if (newChannels >= 1) {
        // Correct to 0-based index in choice
        mInputChannels.Set(newChannels - 1);
    }

    m_inputChannelsChanged.notify();
}

void Au3AudioDevicesProvider::updateInputOutputDevices()
{
    // get api index
    if (audioApiList().size() < 1) {
        return;
    }

    int index = -1;
    auto apiName = currentAudioApi();
    int nHosts = Pa_GetHostApiCount();
    for (int i = 0; i < nHosts; ++i) {
        wxString name = wxSafeConvertMB2WX(Pa_GetHostApiInfo(i)->name);
        if (name == apiName) {
            index = i;
            break;
        }
    }

    const auto currentInputDevice = currentAudioInputDevice();
    const auto inputDevices = audioInputDevices();
    if (!muse::contains(inputDevices, currentInputDevice) && !inputDevices.empty()) {
        // choose the default on this API, as defined by PortAudio
        if (index < 0) {
            setAudioInputDevice(inputDevices[0]);
        } else {
            DeviceSourceMap* defaultMap = DeviceManager::Instance()->GetDefaultInputDevice(index);
            if (defaultMap) {
                setAudioInputDevice(wxToStdSting(MakeDeviceSourceString(defaultMap)));
            }
        }
    }

    const auto currentOutputDevice = currentAudioOutputDevice();
    const auto outputDevices = audioOutputDevices();
    if (!muse::contains(outputDevices, currentOutputDevice) && !outputDevices.empty()) {
        // choose the default on this API, as defined by PortAudio
        if (index < 0) {
            setAudioOutputDevice(outputDevices[0]);
        } else {
            DeviceSourceMap* defaultMap = DeviceManager::Instance()->GetDefaultOutputDevice(index);
            if (defaultMap) {
                setAudioOutputDevice(wxToStdSting(MakeDeviceSourceString(defaultMap)));
            }
        }
    }
}
