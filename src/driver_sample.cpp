#include <openvr_driver.h>
#include "driverlog.h"

#include <vector>
#include <thread>
#include <chrono>
#include <cstring>

#if defined( _WINDOWS )
#include <windows.h>
#endif

using namespace vr;

#include <glm/gtc/quaternion.hpp>
#include <Kinect.h>
IKinectSensor* sensor;      // Kinect sensor
IBodyFrameReader* reader;       // Body frame reader
ICoordinateMapper* mapper;      // Converts between depth, color, and 3d coordinates

// Body tracking variables
bool trackedFirstFrame = true;
BOOLEAN tracked;                    // Do we see a body
Joint joints[JointType_Count];      // List of joints in the tracked body

// Hand tracking variables
HandState leftHandState = HandState_Unknown;
HandState rightHandState = HandState_Unknown;

HRESULT initKinect() {
    HRESULT hr;

    hr = GetDefaultKinectSensor(&sensor);
    if (FAILED(hr)) {
        return hr;
    }

    if (sensor) {
        // Initialize the Kinect and get coordinate mapper and the body reader
        IBodyFrameSource *pBodyFrameSource = NULL;

        hr = sensor->Open();

        if (SUCCEEDED(hr)) {
            hr = sensor->get_CoordinateMapper(&mapper);
        }

        if (SUCCEEDED(hr)) {
            hr = sensor->get_BodyFrameSource(&pBodyFrameSource);
        }

        if (SUCCEEDED(hr)) {
            hr = pBodyFrameSource->OpenReader(&reader);
        }

        pBodyFrameSource->Release();
        pBodyFrameSource = NULL;
    }

    return hr;
}

void processBody(int nBodyCount, IBody** ppBodies) {
    HRESULT hr = S_OK;

    if (SUCCEEDED(hr) && mapper) {
        for (int i = 0; i < nBodyCount; ++i) {
            IBody *pBody = ppBodies[i];
            if (pBody) {
                hr = pBody->get_IsTracked(&tracked);

                if (SUCCEEDED(hr) && tracked) {
                    //Joint joints[JointType_Count];

                    pBody->get_HandLeftState(&leftHandState);
                    pBody->get_HandRightState(&rightHandState);

                    hr = pBody->GetJoints(_countof(joints), joints);
                    if (SUCCEEDED(hr)) {
                        return;
                    }
                }
            }
        }
    }
    trackedFirstFrame = true;
}

void getBodyData() {
    if (!reader) {
        return;
    }

    IBodyFrame *pBodyFrame = NULL;

    HRESULT hr = reader->AcquireLatestFrame(&pBodyFrame);

    if (SUCCEEDED(hr)) {
        IBody *ppBodies[BODY_COUNT] = {0};

        if (SUCCEEDED(hr)) {
            hr = pBodyFrame->GetAndRefreshBodyData(_countof(ppBodies), ppBodies);
        }

        if (SUCCEEDED(hr)) {
            processBody(BODY_COUNT, ppBodies);
        }

        for (int i = 0; i < _countof(ppBodies); ++i) {
            ppBodies[i]->Release();
            ppBodies[i] = NULL;
        }

        pBodyFrame->Release();
        pBodyFrame = NULL;
    }
}

void terminateKinect(){
    mapper->Release();
    mapper = NULL;
    reader->Release();
    reader = NULL;
    sensor->Close();
    sensor = NULL;
}

#if defined(_WIN32)
#define HMD_DLL_EXPORT extern "C" __declspec( dllexport )
#define HMD_DLL_IMPORT extern "C" __declspec( dllimport )
#elif defined(__GNUC__) || defined(COMPILER_GCC) || defined(__APPLE__)
#define HMD_DLL_EXPORT extern "C" __attribute__((visibility("default")))
#define HMD_DLL_IMPORT extern "C"
#else
#error "Unsupported Platform."
#endif

inline HmdQuaternion_t HmdQuaternion_Init( double w, double x, double y, double z )
{
    HmdQuaternion_t quat;
    quat.w = w;
    quat.x = x;
    quat.y = y;
    quat.z = z;
    return quat;
}

inline void HmdMatrix_SetIdentity( HmdMatrix34_t *pMatrix )
{
    pMatrix->m[0][0] = 1.f;
    pMatrix->m[0][1] = 0.f;
    pMatrix->m[0][2] = 0.f;
    pMatrix->m[0][3] = 0.f;
    pMatrix->m[1][0] = 0.f;
    pMatrix->m[1][1] = 1.f;
    pMatrix->m[1][2] = 0.f;
    pMatrix->m[1][3] = 0.f;
    pMatrix->m[2][0] = 0.f;
    pMatrix->m[2][1] = 0.f;
    pMatrix->m[2][2] = 1.f;
    pMatrix->m[2][3] = 0.f;
}


// keys for use with the settings API
static const char * const k_pch_Sample_Section = "driver_sample";
static const char * const k_pch_Sample_SerialNumber_String = "serialNumber";
static const char * const k_pch_Sample_ModelNumber_String = "modelNumber";
static const char * const k_pch_Sample_WindowX_Int32 = "windowX";
static const char * const k_pch_Sample_WindowY_Int32 = "windowY";
static const char * const k_pch_Sample_WindowWidth_Int32 = "windowWidth";
static const char * const k_pch_Sample_WindowHeight_Int32 = "windowHeight";
static const char * const k_pch_Sample_RenderWidth_Int32 = "renderWidth";
static const char * const k_pch_Sample_RenderHeight_Int32 = "renderHeight";
static const char * const k_pch_Sample_SecondsFromVsyncToPhotons_Float = "secondsFromVsyncToPhotons";
static const char * const k_pch_Sample_DisplayFrequency_Float = "displayFrequency";

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

class CWatchdogDriver_Sample : public IVRWatchdogProvider
{
public:
    CWatchdogDriver_Sample()
    {
        m_pWatchdogThread = nullptr;
    }

    virtual EVRInitError Init( vr::IVRDriverContext *pDriverContext ) ;
    virtual void Cleanup() ;

private:
    std::thread *m_pWatchdogThread;
};

CWatchdogDriver_Sample g_watchdogDriverNull;


bool g_bExiting = false;

void WatchdogThreadFunction(  )
{
    while ( !g_bExiting )
    {
#if defined( _WINDOWS )
        // on windows send the event when the Y key is pressed.
        if ( (0x01 & GetAsyncKeyState( 'Y' )) != 0 )
        {
            // Y key was pressed.
            vr::VRWatchdogHost()->WatchdogWakeUp( vr::TrackedDeviceClass_HMD );
        }
        std::this_thread::sleep_for( std::chrono::microseconds( 500 ) );
#else
        // for the other platforms, just send one every five seconds
		std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
		vr::VRWatchdogHost()->WatchdogWakeUp( vr::TrackedDeviceClass_HMD );
#endif
    }
}

EVRInitError CWatchdogDriver_Sample::Init( vr::IVRDriverContext *pDriverContext )
{
    VR_INIT_WATCHDOG_DRIVER_CONTEXT( pDriverContext );
    InitDriverLog( vr::VRDriverLog() );

    // Watchdog mode on Windows starts a thread that listens for the 'Y' key on the keyboard to
    // be pressed. A real driver should wait for a system button event or something else from the
    // the hardware that signals that the VR system should start up.
    g_bExiting = false;
    m_pWatchdogThread = new std::thread( WatchdogThreadFunction );
    if ( !m_pWatchdogThread )
    {
        DriverLog( "Unable to create watchdog thread\n");
        return VRInitError_Driver_Failed;
    }

    return VRInitError_None;
}


void CWatchdogDriver_Sample::Cleanup()
{
    g_bExiting = true;
    if ( m_pWatchdogThread )
    {
        m_pWatchdogThread->join();
        delete m_pWatchdogThread;
        m_pWatchdogThread = nullptr;
    }

    CleanupDriverLog();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSampleDeviceDriver : public vr::ITrackedDeviceServerDriver, public vr::IVRDisplayComponent
{
public:
    CSampleDeviceDriver(  )
    {
        m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
        m_ulPropertyContainer = vr::k_ulInvalidPropertyContainer;

        DriverLog( "Using settings values\n" );
        m_flIPD = vr::VRSettings()->GetFloat( k_pch_SteamVR_Section, k_pch_SteamVR_IPD_Float );

        char buf[1024];
        vr::VRSettings()->GetString( k_pch_Sample_Section, k_pch_Sample_SerialNumber_String, buf, sizeof( buf ) );
        m_sSerialNumber = buf;

        vr::VRSettings()->GetString( k_pch_Sample_Section, k_pch_Sample_ModelNumber_String, buf, sizeof( buf ) );
        m_sModelNumber = buf;

        m_nWindowX = vr::VRSettings()->GetInt32( k_pch_Sample_Section, k_pch_Sample_WindowX_Int32 );
        m_nWindowY = vr::VRSettings()->GetInt32( k_pch_Sample_Section, k_pch_Sample_WindowY_Int32 );
        m_nWindowWidth = vr::VRSettings()->GetInt32( k_pch_Sample_Section, k_pch_Sample_WindowWidth_Int32 );
        m_nWindowHeight = vr::VRSettings()->GetInt32( k_pch_Sample_Section, k_pch_Sample_WindowHeight_Int32 );
        m_nRenderWidth = vr::VRSettings()->GetInt32( k_pch_Sample_Section, k_pch_Sample_RenderWidth_Int32 );
        m_nRenderHeight = vr::VRSettings()->GetInt32( k_pch_Sample_Section, k_pch_Sample_RenderHeight_Int32 );
        m_flSecondsFromVsyncToPhotons = vr::VRSettings()->GetFloat( k_pch_Sample_Section, k_pch_Sample_SecondsFromVsyncToPhotons_Float );
        m_flDisplayFrequency = vr::VRSettings()->GetFloat( k_pch_Sample_Section, k_pch_Sample_DisplayFrequency_Float );

        DriverLog( "driver_null: Serial Number: %s\n", m_sSerialNumber.c_str() );
        DriverLog( "driver_null: Model Number: %s\n", m_sModelNumber.c_str() );
        DriverLog( "driver_null: Window: %d %d %d %d\n", m_nWindowX, m_nWindowY, m_nWindowWidth, m_nWindowHeight );
        DriverLog( "driver_null: Render Target: %d %d\n", m_nRenderWidth, m_nRenderHeight );
        DriverLog( "driver_null: Seconds from Vsync to Photons: %f\n", m_flSecondsFromVsyncToPhotons );
        DriverLog( "driver_null: Display Frequency: %f\n", m_flDisplayFrequency );
        DriverLog( "driver_null: IPD: %f\n", m_flIPD );
    }

    virtual ~CSampleDeviceDriver()
    {
    }


    virtual EVRInitError Activate( vr::TrackedDeviceIndex_t unObjectId )
    {
        m_unObjectId = unObjectId;
        m_ulPropertyContainer = vr::VRProperties()->TrackedDeviceToPropertyContainer( m_unObjectId );


        vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, Prop_ModelNumber_String, m_sModelNumber.c_str() );
        vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, Prop_RenderModelName_String, m_sModelNumber.c_str() );
        vr::VRProperties()->SetFloatProperty( m_ulPropertyContainer, Prop_UserIpdMeters_Float, m_flIPD );
        vr::VRProperties()->SetFloatProperty( m_ulPropertyContainer, Prop_UserHeadToEyeDepthMeters_Float, 0.f );
        vr::VRProperties()->SetFloatProperty( m_ulPropertyContainer, Prop_DisplayFrequency_Float, m_flDisplayFrequency );
        vr::VRProperties()->SetFloatProperty( m_ulPropertyContainer, Prop_SecondsFromVsyncToPhotons_Float, m_flSecondsFromVsyncToPhotons );

        // return a constant that's not 0 (invalid) or 1 (reserved for Oculus)
        vr::VRProperties()->SetUint64Property( m_ulPropertyContainer, Prop_CurrentUniverseId_Uint64, 2 );

        // avoid "not fullscreen" warnings from vrmonitor
        vr::VRProperties()->SetBoolProperty( m_ulPropertyContainer, Prop_IsOnDesktop_Bool, false );

        initKinect();

        return VRInitError_None;
    }

    virtual void Deactivate()
    {
        m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
        terminateKinect();
    }

    virtual void EnterStandby()
    {
    }

    void *GetComponent( const char *pchComponentNameAndVersion )
    {
        if ( !_stricmp( pchComponentNameAndVersion, vr::IVRDisplayComponent_Version ) )
        {
            return (vr::IVRDisplayComponent*)this;
        }

        // override this to add a component to a driver
        return NULL;
    }

    /** debug request from a client */
    virtual void DebugRequest( const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize )
    {
        if( unResponseBufferSize >= 1 )
            pchResponseBuffer[0] = 0;
    }

    virtual void GetWindowBounds( int32_t *pnX, int32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight )
    {
        *pnX = m_nWindowX;
        *pnY = m_nWindowY;
        *pnWidth = m_nWindowWidth;
        *pnHeight = m_nWindowHeight;
    }

    virtual bool IsDisplayOnDesktop()
    {
        return true;
    }

    virtual bool IsDisplayRealDisplay()
    {
        return false;
    }

    virtual void GetRecommendedRenderTargetSize( uint32_t *pnWidth, uint32_t *pnHeight )
    {
        *pnWidth = m_nRenderWidth;
        *pnHeight = m_nRenderHeight;
    }

    virtual void GetEyeOutputViewport( EVREye eEye, uint32_t *pnX, uint32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight )
    {
        *pnY = 0;
        *pnWidth = m_nWindowWidth / 2;
        *pnHeight = m_nWindowHeight;

        if ( eEye == Eye_Left )
        {
            *pnX = 0;
        }
        else
        {
            *pnX = m_nWindowWidth / 2;
        }
    }

    virtual void GetProjectionRaw( EVREye eEye, float *pfLeft, float *pfRight, float *pfTop, float *pfBottom )
    {
        *pfLeft = -1.0;
        *pfRight = 1.0;
        *pfTop = -1.0;
        *pfBottom = 1.0;
    }

    virtual DistortionCoordinates_t ComputeDistortion( EVREye eEye, float fU, float fV )
    {
        DistortionCoordinates_t coordinates;
        coordinates.rfBlue[0] = fU;
        coordinates.rfBlue[1] = fV;
        coordinates.rfGreen[0] = fU;
        coordinates.rfGreen[1] = fV;
        coordinates.rfRed[0] = fU;
        coordinates.rfRed[1] = fV;
        return coordinates;
    }

    virtual DriverPose_t GetPose()
    {
        DriverPose_t pose = { 0 };
        pose.poseIsValid = true;
        pose.result = TrackingResult_Running_OK;
        pose.deviceIsConnected = true;

        pose.qWorldFromDriverRotation = HmdQuaternion_Init( 1, 0, 0, 0 );
        pose.qDriverFromHeadRotation = HmdQuaternion_Init( 1, 0, 0, 0 );

        return pose;
    }

    void RunFrame()
    {
        // In a real driver, this should happen from some pose tracking thread.
        // The RunFrame interval is unspecified and can be very irregular if some other
        // driver blocks it for some periodic task.
        if ( m_unObjectId != vr::k_unTrackedDeviceIndexInvalid )
        {
            getBodyData();
            vr::VRServerDriverHost()->TrackedDevicePoseUpdated( m_unObjectId, GetPose(), sizeof( DriverPose_t ) );
        }
    }

    std::string GetSerialNumber() const { return m_sSerialNumber; }

private:
    vr::TrackedDeviceIndex_t m_unObjectId;
    vr::PropertyContainerHandle_t m_ulPropertyContainer;

    std::string m_sSerialNumber;
    std::string m_sModelNumber;

    int32_t m_nWindowX;
    int32_t m_nWindowY;
    int32_t m_nWindowWidth;
    int32_t m_nWindowHeight;
    int32_t m_nRenderWidth;
    int32_t m_nRenderHeight;
    float m_flSecondsFromVsyncToPhotons;
    float m_flDisplayFrequency;
    float m_flIPD;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSampleControllerDriver : public vr::ITrackedDeviceServerDriver
{
public:
    CSampleControllerDriver(std::string serial)
    {
        m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
        m_ulPropertyContainer = vr::k_ulInvalidPropertyContainer;

        m_sSerialNumber = serial;

        m_sModelNumber = "MyController";

        if(m_sSerialNumber == "CTRL_RIGHT"){
            jHand = JointType_HandRight;
            jTip = JointType_HandTipRight;
            jWrist = JointType_WristRight;
            jElbow = JointType_ElbowRight;
        }
        else if(m_sSerialNumber == "CTRL_LEFT"){
            jHand = JointType_HandLeft;
            jTip = JointType_HandTipLeft;
            jWrist = JointType_WristLeft;
            jElbow = JointType_ElbowLeft;
        }
    }

    virtual ~CSampleControllerDriver()
    {
    }


    virtual EVRInitError Activate( vr::TrackedDeviceIndex_t unObjectId )
    {
        m_unObjectId = unObjectId;
        m_ulPropertyContainer = vr::VRProperties()->TrackedDeviceToPropertyContainer( m_unObjectId );

        vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, Prop_ModelNumber_String, m_sModelNumber.c_str() );
        vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, Prop_RenderModelName_String, m_sModelNumber.c_str() );
        vr::VRProperties()->SetUint64Property( m_ulPropertyContainer, Prop_CurrentUniverseId_Uint64, 2 );

        // avoid "not fullscreen" warnings from vrmonitor
        vr::VRProperties()->SetBoolProperty( m_ulPropertyContainer, Prop_IsOnDesktop_Bool, false );

        // our sample device isn't actually tracked, so set this property to avoid having the icon blink in the status window
        vr::VRProperties()->SetBoolProperty( m_ulPropertyContainer, Prop_NeverTracked_Bool, true );

        if(m_sSerialNumber == "CTRL_RIGHT")
            vr::VRProperties()->SetInt32Property( m_ulPropertyContainer, Prop_ControllerRoleHint_Int32, TrackedControllerRole_RightHand );
        else if(m_sSerialNumber == "CTRL_LEFT")
            vr::VRProperties()->SetInt32Property( m_ulPropertyContainer, Prop_ControllerRoleHint_Int32, TrackedControllerRole_LeftHand );

        vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, Prop_InputProfilePath_String, "{sample}/input/mycontroller_profile.json" );

        // create all the input components
        vr::VRDriverInput()->CreateBooleanComponent( m_ulPropertyContainer, "/input/a/click", &m_compA );
        vr::VRDriverInput()->CreateBooleanComponent( m_ulPropertyContainer, "/input/b/click", &m_compB );
        vr::VRDriverInput()->CreateBooleanComponent( m_ulPropertyContainer, "/input/trigger/click", &m_compTriggerClick );

        VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/trigger/value", &m_compTriggerValue, VRScalarType_Absolute, VRScalarUnits_NormalizedOneSided);

        // create our haptic component
        vr::VRDriverInput()->CreateHapticComponent( m_ulPropertyContainer, "/output/haptic", &m_compHaptic );

        /*
         * Maybe you don't
        //TODO: You know what to do
        VRSettings()->SetString(k_pch_Trackers_Section, "/devices/sample/CTRL_1234", "TrackerRole_RightHand");
        */

        return VRInitError_None;
    }

    virtual void Deactivate()
    {
        m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
    }

    virtual void EnterStandby()
    {
    }

    void *GetComponent( const char *pchComponentNameAndVersion )
    {
        // override this to add a component to a driver
        return NULL;
    }

    /** debug request from a client */
    virtual void DebugRequest( const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize )
    {
        if ( unResponseBufferSize >= 1 )
            pchResponseBuffer[0] = 0;
    }

    virtual DriverPose_t GetPose(){
        DriverPose_t pose = { 0 };
        pose.poseIsValid = true;
        pose.result = TrackingResult_Running_OK;
        pose.deviceIsConnected = true;

        pose.qWorldFromDriverRotation = HmdQuaternion_Init( 1, 0, 0, 0 );
        pose.qDriverFromHeadRotation = HmdQuaternion_Init( 1, 0, 0, 0 );

        const glm::vec3 hand = glm::vec3(joints[jTip].Position.X, joints[jTip].Position.Y, joints[jTip].Position.Z);
        const glm::vec3 elbow = glm::vec3(joints[jElbow].Position.X, joints[jElbow].Position.Y, joints[jElbow].Position.Z);
        const glm::vec3 wrist = glm::vec3(joints[jWrist].Position.X, joints[jWrist].Position.Y, joints[jWrist].Position.Z);

        const glm::vec3 direction = glm::normalize(hand - wrist);
        const glm::quat rotation = glm::quatLookAt(direction, glm::vec3(0, 1, 0));

        pose.vecPosition[0] = joints[jHand].Position.X - joinPos.x;
        pose.vecPosition[1] = joints[jHand].Position.Y - joinPos.y;
        pose.vecPosition[2] = joints[jHand].Position.Z - joinPos.z;

        pose.qRotation.w = rotation.w;
        pose.qRotation.x = rotation.x;
        pose.qRotation.y = rotation.y;
        pose.qRotation.z = rotation.z;

        return pose;
    }

    void RunFrame() {
        /*if(tracked && trackedFirstFrame){
            joinPos = glm::vec3(joints[jHand].Position.X, joints[jHand].Position.Y, joints[jHand].Position.Z);
            trackedFirstFrame = false;
        }*/

        if(leftHandState == HandState_Open) {
            VRDriverInput()->UpdateScalarComponent(m_compTriggerValue, 1.0, 0);
            vr::VRDriverInput()->UpdateBooleanComponent(m_compTriggerClick, true, 0);
        }
        else {
            VRDriverInput()->UpdateScalarComponent(m_compTriggerValue, 0.0, 0);
            vr::VRDriverInput()->UpdateBooleanComponent(m_compTriggerClick, false, 0);
        }

        VRServerDriverHost()->TrackedDevicePoseUpdated(m_unObjectId, GetPose(), sizeof(DriverPose_t));
    }

    void ProcessEvent( const vr::VREvent_t & vrEvent )
    {
        switch ( vrEvent.eventType )
        {
            case vr::VREvent_Input_HapticVibration:
            {
                if ( vrEvent.data.hapticVibration.componentHandle == m_compHaptic )
                {
                    // This is where you would send a signal to your hardware to trigger actual haptic feedback
                    DriverLog( "BUZZ!\n" );
                }
            }
                break;
        }
    }


    std::string GetSerialNumber() const { return m_sSerialNumber; }

private:
    vr::TrackedDeviceIndex_t m_unObjectId;
    vr::PropertyContainerHandle_t m_ulPropertyContainer;

    vr::VRInputComponentHandle_t m_compA;
    vr::VRInputComponentHandle_t m_compB;
    vr::VRInputComponentHandle_t m_compTriggerValue;
    vr::VRInputComponentHandle_t m_compTriggerClick;
    vr::VRInputComponentHandle_t m_compHaptic;

    std::string m_sSerialNumber;
    std::string m_sModelNumber;

    JointType jHand, jTip, jWrist, jElbow;

    glm::vec3 joinPos{0,0,1.4};
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CServerDriver_Sample: public IServerTrackedDeviceProvider
{
public:
    virtual EVRInitError Init( vr::IVRDriverContext *pDriverContext ) ;
    virtual void Cleanup() ;
    virtual const char * const *GetInterfaceVersions() { return vr::k_InterfaceVersions; }
    virtual void RunFrame() ;
    virtual bool ShouldBlockStandbyMode()  { return true; }
    virtual void EnterStandby()  {}
    virtual void LeaveStandby()  {}

private:
    CSampleDeviceDriver *m_pNullHmdLatest = nullptr;
    CSampleControllerDriver *m_pControllerRight = nullptr;
    CSampleControllerDriver *m_pControllerLeft = nullptr;
};

CServerDriver_Sample g_serverDriverNull;


EVRInitError CServerDriver_Sample::Init( vr::IVRDriverContext *pDriverContext )
{
    VR_INIT_SERVER_DRIVER_CONTEXT( pDriverContext );
    InitDriverLog( vr::VRDriverLog() );

    m_pNullHmdLatest = new CSampleDeviceDriver();
    vr::VRServerDriverHost()->TrackedDeviceAdded( m_pNullHmdLatest->GetSerialNumber().c_str(), vr::TrackedDeviceClass_HMD, m_pNullHmdLatest );

    m_pControllerRight = new CSampleControllerDriver("CTRL_RIGHT");
    vr::VRServerDriverHost()->TrackedDeviceAdded( m_pControllerRight->GetSerialNumber().c_str(), vr::TrackedDeviceClass_Controller, m_pControllerRight );

    m_pControllerLeft = new CSampleControllerDriver("CTRL_LEFT");
    vr::VRServerDriverHost()->TrackedDeviceAdded( m_pControllerLeft->GetSerialNumber().c_str(), vr::TrackedDeviceClass_Controller, m_pControllerLeft );

    return VRInitError_None;
}

void CServerDriver_Sample::Cleanup()
{
    CleanupDriverLog();
    delete m_pNullHmdLatest;
    m_pNullHmdLatest = NULL;
    delete m_pControllerRight;
    m_pControllerRight = NULL;
    delete m_pControllerLeft;
    m_pControllerLeft = NULL;
}


void CServerDriver_Sample::RunFrame()
{
    if ( m_pNullHmdLatest ) m_pNullHmdLatest->RunFrame();
    if ( m_pControllerRight ) m_pControllerRight->RunFrame();
    if ( m_pControllerLeft ) m_pControllerLeft->RunFrame();

    vr::VREvent_t vrEvent;
    while ( vr::VRServerDriverHost()->PollNextEvent( &vrEvent, sizeof( vrEvent ) ) )
    {
        if ( m_pControllerRight ) m_pControllerRight->ProcessEvent( vrEvent );
        if ( m_pControllerLeft ) m_pControllerLeft->ProcessEvent( vrEvent );
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
HMD_DLL_EXPORT void *HmdDriverFactory( const char *pInterfaceName, int *pReturnCode )
{
    if( 0 == strcmp( IServerTrackedDeviceProvider_Version, pInterfaceName ) )
    {
        return &g_serverDriverNull;
    }
    if( 0 == strcmp( IVRWatchdogProvider_Version, pInterfaceName ) )
    {
        return &g_watchdogDriverNull;
    }

    if( pReturnCode )
        *pReturnCode = VRInitError_Init_InterfaceNotFound;

    return NULL;
}