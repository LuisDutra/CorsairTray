#include <iostream>
#include <thread>
#include <chrono>
#include <string>

#include "CorsairCUESDK.h" // seu header aí

// Helpers para imprimir enums (bem simples, pode melhorar se quiser)
const char *deviceTypeToString(CorsairDeviceType t)
{
    switch (t)
    {
    case CDT_Keyboard:
        return "Keyboard";
    case CDT_Mouse:
        return "Mouse";
    case CDT_Mousemat:
        return "Mousemat";
    case CDT_Headset:
        return "Headset";
    case CDT_HeadsetStand:
        return "HeadsetStand";
    case CDT_FanLedController:
        return "FanLedController";
    case CDT_LedController:
        return "LedController";
    case CDT_MemoryModule:
        return "MemoryModule";
    case CDT_Cooler:
        return "Cooler";
    case CDT_Motherboard:
        return "Motherboard";
    case CDT_GraphicsCard:
        return "GraphicsCard";
    case CDT_Touchbar:
        return "Touchbar";
    case CDT_GameController:
        return "GameController";
    default:
        return "Unknown";
    }
}

const char *propertyIdToString(CorsairDevicePropertyId id)
{
    switch (id)
    {
    case CDPI_PropertyArray:
        return "PropertyArray";
    case CDPI_MicEnabled:
        return "MicEnabled";
    case CDPI_SurroundSoundEnabled:
        return "SurroundSoundEnabled";
    case CDPI_SidetoneEnabled:
        return "SidetoneEnabled";
    case CDPI_EqualizerPreset:
        return "EqualizerPreset";
    case CDPI_PhysicalLayout:
        return "PhysicalLayout";
    case CDPI_LogicalLayout:
        return "LogicalLayout";
    case CDPI_MacroKeyArray:
        return "MacroKeyArray";
    case CDPI_BatteryLevel:
        return "BatteryLevel";
    case CDPI_ChannelLedCount:
        return "ChannelLedCount";
    case CDPI_ChannelDeviceCount:
        return "ChannelDeviceCount";
    case CDPI_ChannelDeviceLedCountArray:
        return "ChannelDeviceLedCountArray";
    case CDPI_ChannelDeviceTypeArray:
        return "ChannelDeviceTypeArray";
    default:
        return "UnknownProperty";
    }
}

// Imprime um CorsairProperty genérico
void printPropertyValue(const CorsairProperty &prop)
{
    switch (prop.type)
    {
    case CT_Boolean:
        std::cout << (prop.value.boolean ? "true" : "false");
        break;
    case CT_Int32:
        std::cout << prop.value.int32;
        break;
    case CT_Float64:
        std::cout << prop.value.float64;
        break;
    case CT_String:
        std::cout << (prop.value.string ? prop.value.string : "");
        break;

    case CT_Boolean_Array:
        std::cout << "[";
        for (unsigned int i = 0; i < prop.value.boolean_array.count; ++i)
        {
            std::cout << (prop.value.boolean_array.items[i] ? "true" : "false");
            if (i + 1 < prop.value.boolean_array.count)
                std::cout << ", ";
        }
        std::cout << "]";
        break;

    case CT_Int32_Array:
        std::cout << "[";
        for (unsigned int i = 0; i < prop.value.int32_array.count; ++i)
        {
            std::cout << prop.value.int32_array.items[i];
            if (i + 1 < prop.value.int32_array.count)
                std::cout << ", ";
        }
        std::cout << "]";
        break;

    case CT_Float64_Array:
        std::cout << "[";
        for (unsigned int i = 0; i < prop.value.float64_array.count; ++i)
        {
            std::cout << prop.value.float64_array.items[i];
            if (i + 1 < prop.value.float64_array.count)
                std::cout << ", ";
        }
        std::cout << "]";
        break;

    case CT_String_Array:
        std::cout << "[";
        for (unsigned int i = 0; i < prop.value.string_array.count; ++i)
        {
            const char *s = prop.value.string_array.items[i];
            std::cout << "\"" << (s ? s : "") << "\"";
            if (i + 1 < prop.value.string_array.count)
                std::cout << ", ";
        }
        std::cout << "]";
        break;

    default:
        std::cout << "<tipo desconhecido " << prop.type << ">";
        break;
    }
}

// Lê e imprime TODAS as propriedades suportadas por um device
void dumpDeviceProperties(const CorsairDeviceInfo &dev)
{
    std::cout << "  [Propriedades]\n";

    // 1) Primeiro, ler o array de propriedades suportadas
    CorsairProperty propArray{};
    CorsairError err = CorsairReadDeviceProperty(
        dev.id,
        CDPI_PropertyArray,
        0,
        &propArray);

    if (err != CE_Success || propArray.type != CT_Int32_Array)
    {
        std::cout << "    Não foi possível ler CDPI_PropertyArray (err=" << err << ")\n";
        CorsairFreeProperty(&propArray);
        return;
    }

    auto &arr = propArray.value.int32_array;

    std::cout << "    Propriedades suportadas (" << arr.count << "):\n";

    for (unsigned int i = 0; i < arr.count; ++i)
    {
        auto pid = static_cast<CorsairDevicePropertyId>(arr.items[i]);

        // 2) Para cada propertyId, pegar info (tipo + flags)
        CorsairDataType dataType{};
        unsigned int flags = 0;
        err = CorsairGetDevicePropertyInfo(dev.id, pid, 0, &dataType, &flags);
        if (err != CE_Success)
        {
            std::cout << "    - " << propertyIdToString(pid)
                      << " (id=" << pid << "): GetDevicePropertyInfo falhou (err=" << err << ")\n";
            continue;
        }

        std::cout << "    - " << propertyIdToString(pid) << " (id=" << pid << "): ";

        if (!(flags & CPF_CanRead))
        {
            std::cout << "<não legível>\n";
            continue;
        }

        // 3) Ler o valor real da propriedade
        CorsairProperty p{};
        err = CorsairReadDeviceProperty(dev.id, pid, 0, &p);
        if (err != CE_Success)
        {
            std::cout << "<erro ao ler, err=" << err << ">\n";
            continue;
        }

        printPropertyValue(p);
        std::cout << "\n";

        CorsairFreeProperty(&p);
    }

    // liberar o array de propriedades
    CorsairFreeProperty(&propArray);
}

// Callback de estado da sessão (opcional)
void onSessionStateChanged(void * /*ctx*/, const CorsairSessionStateChanged *s)
{
    if (!s)
        return;
    std::cout << "[Sessão] Novo estado: " << s->state << "\n";
}

int main()
{
    // 1) Conectar ao iCUE
    CorsairError err = CorsairConnect(&onSessionStateChanged, nullptr);
    if (err != CE_Success)
    {
        std::cout << "Erro ao conectar ao iCUE: " << err << "\n";
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Opcional: detalhes da sessão
    CorsairSessionDetails details{};
    if (CorsairGetSessionDetails(&details) == CE_Success)
    {
        std::cout << "SDK client: "
                  << details.clientVersion.major << "."
                  << details.clientVersion.minor << "."
                  << details.clientVersion.patch << "\n";
        std::cout << "iCUE host: "
                  << details.serverHostVersion.major << "."
                  << details.serverHostVersion.minor << "."
                  << details.serverHostVersion.patch << "\n\n";
    }

    // 2) Buscar todos os devices
    CorsairDeviceFilter filter{};
    filter.deviceTypeMask = CDT_All;

    CorsairDeviceInfo devices[CORSAIR_DEVICE_COUNT_MAX];
    int count = 0;

    err = CorsairGetDevices(&filter, CORSAIR_DEVICE_COUNT_MAX, devices, &count);
    if (err != CE_Success)
    {
        std::cout << "Erro ao obter devices: " << err << "\n";
        return 1;
    }

    std::cout << "Devices encontrados: " << count << "\n\n";

    // 3) Dump de cada device
    for (int i = 0; i < count; ++i)
    {
        const auto &dev = devices[i];

        std::cout << "====================================================\n";
        std::cout << "Device #" << i << "\n";
        std::cout << "  Type:          " << deviceTypeToString(dev.type) << " (" << dev.type << ")\n";
        std::cout << "  Model:         " << dev.model << "\n";
        std::cout << "  Serial:        " << dev.serial << "\n";
        std::cout << "  Id:            " << dev.id << "\n";
        std::cout << "  LedCount:      " << dev.ledCount << "\n";
        std::cout << "  ChannelCount:  " << dev.channelCount << "\n";

        // 4) Agora, todas as propriedades suportadas (inclui BatteryLevel, MicEnabled, etc.)
        dumpDeviceProperties(dev);

        std::cout << "\n";
    }

    CorsairDisconnect();
    return 0;
}
