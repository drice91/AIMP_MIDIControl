#include "pch.h"
#include <windows.h>
#include "apiPlugin.h"
#include "apiCore.h"
#include "apiPlayer.h"
#include "RtMidi.h"

class MidiPlugin : public IAIMPPlugin
{

public:
    MidiPlugin() : m_refCount(1) {}

    // IUnknown
    ULONG WINAPI AddRef() override { return ++m_refCount; }

    ULONG WINAPI Release() override
    {
        if (--m_refCount == 0) {
            delete this;
            return 0;
        }
        return m_refCount;
    }

    HRESULT WINAPI QueryInterface(REFIID riid, void** ppv) override
    {

        wchar_t guidString[64];
        StringFromGUID2(riid, guidString, 64);
        OutputDebugString(L"[AIMP_MIDIControl] QueryInterface called with IID: ");
        OutputDebugString(guidString);
        OutputDebugString(L"\n");

        if (riid == IID_IUnknown)
        {
            *ppv = static_cast<IAIMPPlugin*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    // IAIMPPlugin
    TChar* WINAPI InfoGet(int Index) override
    {
        switch (Index) {
        case AIMP_PLUGIN_INFO_NAME:
            return (TChar*)L"MIDIControl";
        case AIMP_PLUGIN_INFO_AUTHOR:
            return (TChar*)L"Andreas Thiede";
        case AIMP_PLUGIN_INFO_SHORT_DESCRIPTION:
            return (TChar*)L"Control AIMP using MIDI Commands";
        }
        return nullptr;
    }

    DWORD WINAPI InfoGetCategories() override
    {
        return AIMP_PLUGIN_CATEGORY_ADDONS;
    }

    HRESULT WINAPI Initialize(IAIMPCore* pAIMPCore) override
    {
        m_pAIMPCore = pAIMPCore;
        OutputDebugString(L"[AIMP_MIDIControl] Initialize called\n");

        if (FAILED(m_pAIMPCore->QueryInterface(IID_IAIMPServicePlayer, (void**)&m_pPlayer)))
        {
            OutputDebugString(L"[MidiPlugin] Failed to get IAIMPServicePlayer\n");
            return E_FAIL;
        }

        try {
            m_pMidiIn = new RtMidiIn();

            if (m_pMidiIn->getPortCount() == 0)
            {
                OutputDebugString(L"[AIMP_MIDIControl] No MIDI ports available\n");
                delete m_pMidiIn;
                m_pMidiIn = nullptr;
                return S_FALSE; // still initialize plugin, just no MIDI
            }

            m_pMidiIn->openPort(0);
            m_pMidiIn->setCallback(&MidiCallback, this);
            m_pMidiIn->ignoreTypes(false, false, false);

            OutputDebugString(L"[MidiPlugin] MIDI input initialized\n");
        }
        catch (RtMidiError& e)
        {
            OutputDebugStringA(("[MidiPlugin] RtMidiError: " + e.getMessage() + "\n").c_str());
            return E_FAIL;
        }


        return S_OK;
    }

    HRESULT WINAPI Finalize() override
    {
        OutputDebugString(L"[AIMP_MIDIControl] Finalize called\n");

        if (m_pPlayer) {
            m_pPlayer->Release();
            m_pPlayer = nullptr;
        }

        if (m_pMidiIn) {
            delete m_pMidiIn;
            m_pMidiIn = nullptr;
        }


        return S_OK;
    }

    void WINAPI SystemNotification(int NotifyID, IUnknown* Data) override
    {
        OutputDebugString(L"[AIMP_MIDIControl] SystemNotification received\n");
    }

    static void MidiCallback(double deltatime, std::vector<unsigned char>* pMessage, void* userData)
    {

        MidiPlugin* plugin = static_cast<MidiPlugin*>(userData);

        if (!plugin || !plugin->m_pPlayer || !pMessage || pMessage->size() < 2)
            return;

        unsigned char status = pMessage->at(0);
        unsigned char data1 = pMessage->at(1);
        unsigned char data2 = pMessage->size() >= 3 ? pMessage->at(2) : 0;

        // 1CCCNNNN = status (CCC=command-nr, NNNN=channel-nr)
        // 0VVVVVVV = data (VVVVVVV=value 0-127)

        // xF0 = 11110000   (mask for highest 4 bits)
        // x90 = 10010000   (1=status, 001=command[Note On event], 0000=channel-nr.)
        // xB0 = 10110000   (1=status, 011=command[control change], 0000=channel-nr.)

        if ((status & 0xF0) == 0x90)    // Note-On event
        {
            // Note on event
            switch (data1)
            {
            case 60: plugin->m_pPlayer->Pause(); break;       // C3 (middle C)
            case 61: plugin->m_pPlayer->Resume(); break;      // C#3
            case 62: plugin->m_pPlayer->GoToNext(); break;    // D3
            case 63: plugin->m_pPlayer->GoToPrev(); break;    // D#3
            }
        }
        else if ((status & 0xF0) == 0xB0)   
        {
            // Control change - Volume
            if (data1 == 0x07)
            {
                float newVolumne = static_cast<float>(data2) / 127.0f;
                plugin->m_pPlayer->SetVolume(newVolumne);
            }
        }
    }

private:
    ULONG m_refCount;
    IAIMPCore* m_pAIMPCore = nullptr;
    IAIMPServicePlayer* m_pPlayer = nullptr;
    RtMidiIn* m_pMidiIn = nullptr;

};

extern "C" HRESULT AIMPPluginGetHeader(IAIMPPlugin** Header)
{
    OutputDebugString(L"[AIMP_MIDIControl] Header called\n");
    *Header = new MidiPlugin();
    return S_OK;
}