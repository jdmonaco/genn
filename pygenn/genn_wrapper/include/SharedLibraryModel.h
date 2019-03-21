
#pragma once

// Standard C++ includes
#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <bitset>

#ifdef _WIN32
#include <windows.h>
#else
// POSIX C includes
extern "C"
{
#include <dlfcn.h>
}
#endif

//----------------------------------------------------------------------------
// SharedLibraryModel
//----------------------------------------------------------------------------
template<typename scalar = float>
class SharedLibraryModel
{
public:
    SharedLibraryModel()
    :   m_Library(nullptr), m_AllocateMem(nullptr), m_FreeMem(nullptr),
        m_Initialize(nullptr), m_InitializeSparse(nullptr), m_StepTime(nullptr)
    {
    }

    SharedLibraryModel(const std::string &pathToModel, const std::string &modelName)
    {
        if(!open(pathToModel, modelName)) {
            throw std::runtime_error("Unable to open library");
        }
    }

    ~SharedLibraryModel()
    {
        // Close model library if loaded successfully
        if(m_Library) {
            freeMem();
#ifdef _WIN32
            FreeLibrary(m_Library);
#else
            dlclose(m_Library);
#endif
        }
    }

    //----------------------------------------------------------------------------
    // Public API
    //----------------------------------------------------------------------------
    bool open(const std::string &pathToModel, const std::string &modelName)
    {
#ifdef _WIN32
        const std::string libraryName = pathToModel + "\\runner_Release.dll";
        m_Library = LoadLibrary(libraryName.c_str());
#else
        const std::string libraryName = pathToModel + modelName + "_CODE/librunner.so";
        m_Library = dlopen(libraryName.c_str(), RTLD_NOW);
#endif

        // If it fails throw
        if(m_Library != nullptr) {
            m_AllocateMem = (VoidFunction)getSymbol("allocateMem");
            m_FreeMem = (VoidFunction)getSymbol("freeMem");

            m_Initialize = (VoidFunction)getSymbol("initialize");
            m_InitializeSparse = (VoidFunction)getSymbol("initializeSparse");

            m_StepTime = (VoidFunction)getSymbol("stepTime");

            return true;
        }
        else {
#ifdef _WIN32
            std::cerr << "Unable to load library - error:" << std::to_string(GetLastError()) << std::endl;;
#else
            std::cerr << "Unable to load library - error:" << dlerror() << std::endl;
#endif
            return false;
        }
    }

    void pullStateFromDevice(const std::string &popName)
    {
        // Get push and pull state functions and check pull exists
        const auto pushPull = getPopPushPullFunction(popName, "State");
        if(pushPull.second == nullptr) {
            throw std::runtime_error("You cannot pull state from population '" + popName + "'");
        }

        // Call pull
        pushPull.second();
    }
    
    void pullSpikesFromDevice(const std::string &popName)
    {
        // Get push and pull spikes functions and check pull exists
        const auto pushPull = getPopPushPullFunction(popName, "Spikes");
        if(pushPull.second == nullptr) {
            throw std::runtime_error("You cannot pull spikes from population '" + popName + "'");
        }

        // Call pull
        pushPull.second();
    }
    
    void pullCurrentSpikesFromDevice(const std::string &popName)
    {
        // Get push and pull spikes functions and check pull exists
        const auto pushPull = getPopPushPullFunction(popName, "CurrentSpikes");
        if(pushPull.second == nullptr) {
            throw std::runtime_error("You cannot pull current spikes from population '" + popName + "'");
        }

        // Call pull
        pushPull.second();
    }

    void pullConnectivityFromDevice(const std::string &popName)
    {
        // Get push and pull connectivity functions and check pull exists
        const auto pushPull = getPopPushPullFunction(popName, "Connectivity");
        if(pushPull.second == nullptr) {
            throw std::runtime_error("You cannot pull connectivity from population '" + popName + "'");
        }

        // Call pull
        pushPull.second();
    }

    void pullVarFromDevice(const std::string &popName, const std::string &varName)
    {
        // Get push and pull connectivity functions and check pull exists
        const auto pushPull = getPopPushPullFunction(popName, varName);
        if(pushPull.second == nullptr) {
            throw std::runtime_error("You cannot pull var '" + varName + "' from population '" + popName + "'");
        }

        // Call pull
        pushPull.second();
    }

    void pushStateToDevice(const std::string &popName, bool uninitialisedOnly = false)
    {
        // Get push and pull state functions and check pull exists
        const auto pushPull = getPopPushPullFunction(popName, "State");
        if(pushPull.first == nullptr) {
            throw std::runtime_error("You cannot push state to population '" + popName + "'");
        }

        // Call push
        pushPull.first(uninitialisedOnly);
    }

    void pushSpikesToDevice(const std::string &popName, bool uninitialisedOnly = false)
    {
        // Get push and pull spikes functions and check pull exists
        const auto pushPull = getPopPushPullFunction(popName, "Spikes");
        if(pushPull.first == nullptr) {
            throw std::runtime_error("You cannot push spikes to population '" + popName + "'");
        }

        // Call push
        pushPull.first(uninitialisedOnly);
    }

    void pushCurrentSpikesToDevice(const std::string &popName, bool uninitialisedOnly = false)
    {
        // Get push and pull spikes functions and check pull exists
        const auto pushPull = getPopPushPullFunction(popName, "CurrentSpikes");
        if(pushPull.first == nullptr) {
            throw std::runtime_error("You cannot push current spikes to population '" + popName + "'");
        }

        // Call push
        pushPull.first(uninitialisedOnly);
    }

    void pushConnectivityToDevice(const std::string &popName, bool uninitialisedOnly = false)
    {
        // Get push and pull connectivity functions and check pull exists
        const auto pushPull = getPopPushPullFunction(popName, "Connectivity");
        if(pushPull.first == nullptr) {
            throw std::runtime_error("You cannot push connectivity to population '" + popName + "'");
        }

        // Call push
        pushPull.first(uninitialisedOnly);
    }

    void pushVarToDevice(const std::string &popName, const std::string &varName, bool uninitialisedOnly = false)
    {
        // Get push and pull connectivity functions and check pull exists
        const auto pushPull = getPopPushPullFunction(popName, varName);
        if(pushPull.first == nullptr) {
            throw std::runtime_error("You cannot push var '" + varName + "' to population '" + popName + "'");
        }

        // Call push
        pushPull.first(uninitialisedOnly);
    }

    // Assign symbol from shared model to the provided pointer.
    // The symbol is supposed to be an array
    // When used with numpy, wrapper automatically provides varPtr and n1
    template <typename T>
    void assignExternalPointerArray( const std::string &varName,
                                     const int varSize,
                                     T** varPtr, int* n1 )
    {
        *varPtr = *( static_cast<T**>( getSymbol( varName ) ) );
        *n1 = varSize;
    }
    
    // Assign symbol from shared model to the provided pointer.
    // The symbol is supposed to be a single value
    // When used with numpy, wrapper automatically provides varPtr and n1
    template <typename T>
    void assignExternalPointerSingle( const std::string &varName,
                                      T** varPtr, int* n1 )
    {
      *varPtr = static_cast<T*>( getSymbol( varName ) );
      *n1 = 1;
    }

    void allocateMem()
    {
        m_AllocateMem();
    }

    void freeMem()
    {
        m_FreeMem();
    }

   /* **TODO** the backend should generate these functions for EGP variables with * in
    * void allocateExtraGlobalParam( const std::string &popName,
                                   const std::string &paramName,
                                   const int size )
    {
        auto egp = static_cast<void**>(getSymbol( paramName + popName ));
#ifndef CPU_ONLY
        CHECK_CUDA_ERRORS( cudaHostAlloc( egp, size * sizeof( scalar ), cudaHostAllocPortable ) );
#else
        *egp = malloc( size * sizeof( scalar ) );
#endif
    }

    void freeExtraGlobalParam( const std::string &popName,
                               const std::string &paramName )
    {
        auto egp = static_cast<void**>( getSymbol( paramName + popName ));
#ifndef CPU_ONLY
        CHECK_CUDA_ERRORS( cudaFreeHost( *egp ) );
#else
        free(*egp);
#endif
    }*/

    void initialize()
    {
        m_Initialize();
    }

    void initializeSparse()
    {
        m_InitializeSparse();
    }

    void stepTime()
    {
        m_StepTime();
    }

private:
    //----------------------------------------------------------------------------
    // Typedefines
    //----------------------------------------------------------------------------
    typedef void (*VoidFunction)(void);
    typedef void (*PushFunction)(bool);
    typedef void (*PullFunction)(void);

    typedef std::pair<PushFunction, PullFunction> PushPullFunc;

    //----------------------------------------------------------------------------
    // Private methods
    //----------------------------------------------------------------------------
    PushPullFunc getPopPushPullFunction(const std::string &popName, const std::string &varName)
    {
        // Get map of variables associated with population
        // **NOTE** this may CREATE this map
        auto &popVars = m_PopulationsIO[popName];

        // If var is found, return associated push and pull functions
        const auto popVar = popVars.find(varName);
        if(popVar != popVars.end()) {
            return popVar->second;
        }
        else {
            // Get symbols for push and pull functions
            auto pushFunc = (PushFunction)getSymbol("push" + popName + varName + "ToDevice", true);
            auto pullFunc = (PullFunction)getSymbol("pull" + popName + varName + "FromDevice", true);

            // Add to map
            auto newPopVar = popVars.emplace(std::piecewise_construct,
                                             std::forward_as_tuple(varName),
                                             std::forward_as_tuple(pushFunc, pullFunc));

            // Return newly added push and pull functions
            return newPopVar.first->second;
        }
    }

    void *getSymbol(const std::string &symbolName, bool allowMissing = false, void *defaultSymbol = nullptr)
    {
#ifdef _WIN32
        void *symbol = GetProcAddress(m_Library, symbolName.c_str());
#else
        void *symbol = dlsym(m_Library, symbolName.c_str());
#endif

        // If this symbol's missing
        if(symbol == nullptr) {
            // If this isn't allowed, throw error
            if(!allowMissing) {
                throw std::runtime_error("Cannot find symbol '" + symbolName + "'");
            }
            // Otherwise, return default
            else {
                return defaultSymbol;
            }
        }
        // Otherwise, return symbolPopulationFuncs
        else {
            return symbol;
        }
    }

    //----------------------------------------------------------------------------
    // Members
    //----------------------------------------------------------------------------
#ifdef _WIN32
    HMODULE m_Library;
#else
    void *m_Library;
#endif

    VoidFunction m_AllocateMem;
    VoidFunction m_FreeMem;
    VoidFunction m_Initialize;
    VoidFunction m_InitializeSparse;
    VoidFunction m_StepTime;
    
    std::unordered_map<std::string, std::unordered_map<std::string, PushPullFunc>> m_PopulationsIO;
};
