#include "RunnerInterface.hpp"




Runner::Runner(const int& portNumber)
{
    InitEnv();
    BuildKernel();
    if(!ServerInit(portNumber))
    {
        std::cerr<<"ERR on server init"<<std::endl;
    }
}


Runner::~Runner()
{
    UnInit();
    if(env != nullptr)
    {
        delete env;
    }
    if(kens != nullptr)
    {
        delete kens;
    }
    if(server != nullptr)
    {
        server->stop();
        delete server;
    }
}
MAYBE_DATA
Runner::AccessData(FFTRequest& req)
{
    auto mempath = req.GetSharedMemPath();
    if(mempath.has_value())
    {
        dataInfo = req.GetSHMPtr();
        if(!dataInfo.has_value())
        {
            return std::nullopt;
        }
        else
        {
            VECF accResult(req.get_dataLength());
            memcpy( accResult.data(), 
                    dataInfo.value().first, 
                    req.get_dataLength() * sizeof(float));
            return std::move(accResult);
        }
        
    }
    else
    {
        return std::move(req.GetData());
    }
}

bool
Runner::ServerInit(const int& pNum)
{
    if(server != nullptr)
    {
        delete server;
    }
    server = new ix::WebSocketServer(pNum, "0.0.0.0");//need to add random
    server->setOnClientMessageCallback([&](
            std::shared_ptr<ix::ConnectionState> connectionState,
            ix::WebSocket &webSocket,
            const ix::WebSocketMessagePtr& msg)
            {
                if(msg->type == ix::WebSocketMessageType::Open)
                {
                }
                if(msg->type == ix::WebSocketMessageType::Message)
                {
                    if(msg->binary)
                    {
                        
                        FFTRequest received(msg->str);
                        received.MakeWField();
                        auto data = AccessData(received);
                        if(data.has_value())
                        {
                            auto result =
                            ActivateSTFT(   data.value(), 
                                            received.get_WindowSizeEXP(),
                                            received.get_OverlapRate(),
                                            received.GetOption());
                            if(dataInfo.has_value() && result.has_value())
                            {
                                memcpy( dataInfo.value().first, 
                                        result.value().data(),
                                        result.value().size() * sizeof(float));
                            }
                            else if(result.has_value())
                            {
                                received.SetData(result.value());
                            }
                            else
                            {
                                received.StoreErrorMessage();
                            }
                            if(dataInfo.has_value())
                            {
                                received.FreeSHMPtr(dataInfo.value());
                            }
                        }
                        else//error message
                        {
                            received.StoreErrorMessage();
                        }
                        auto serialResult = received.Serialize();
                        if(serialResult.has_value())
                        {
                            webSocket.sendBinary(serialResult.value());
                        }
                    }
                    else
                    {
                        if(msg->str == "CLOSE_REQUEST")
                        {
                            webSocket.sendText("CLOSE_COMPLETE");
                            std::thread([&]() {
                            server->stop();
                            }).detach();
                            
                        }
                    }
                }
            }
        );
    auto err = server->listen();
    if(!err.first)
    {
        return false;
    }
    server->start();
    return true;
}

int main(int argc, char *argv[])
{
    int portNumber = 54500; //default portnumber
    if(argc == 2)
    {
        try
        {
            portNumber = std::stoi(argv[1]);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            return 1;
        }
    }
    ix::initNetSystem();
    Runner mainOBJ = Runner(portNumber);
    mainOBJ.server->wait();
    ix::uninitNetSystem();
    return 0;
}

#define I_OPTION_CHECK(FUNC, OPT)\
if(options.find(OPT) != std::string::npos)\
{\
    if(!FUNC)\
    {\
        return std::move(OPT);\
    }\
}

#define EI_OPTION_CHECK(FUNC, OPT)\
else if(options.find(OPT) != std::string::npos)\
{\
    if(!FUNC)\
    {\
        return std::move(OPT);\
    }\
}

std::string
runnerFunction::Default_Pipeline(
    void* userStruct, 
    void* origin,
    void* real,
    void* imag,
    void* subreal,
    void* subimag,
    void* out,
    CUI   FullSize,
    CUI   windowSize,
    CUI   qtConst,
    CUI   OFullSize,
    CUI   OHalfSize,
    CUI   OMove,
    const std::string&  options,
    const int           windowSizeEXP,
    const float         overlapRatio
)
{
    

    runnerFunction::Overlap
    (
        userStruct,
        origin,
        OFullSize,
        FullSize,
        windowSizeEXP,
        OMove,
        real
    );
    I_OPTION_CHECK ( runnerFunction::Hanning            (userStruct, real, OFullSize, windowSize), "--hanning_window"           )
    EI_OPTION_CHECK( runnerFunction::Hamming            (userStruct, real, OFullSize, windowSize), "--hamming_window"           )
    EI_OPTION_CHECK( runnerFunction::Blackman           (userStruct, real, OFullSize, windowSize), "--blackman_window"          )
    EI_OPTION_CHECK( runnerFunction::Nuttall            (userStruct, real, OFullSize, windowSize), "--nuttall_window"           )
    EI_OPTION_CHECK( runnerFunction::Blackman_Nuttall   (userStruct, real, OFullSize, windowSize), "--blackman_nuttall_window"  )
    EI_OPTION_CHECK( runnerFunction::Blackman_Harris    (userStruct, real, OFullSize, windowSize), "--blackman_harris_window"   )
    EI_OPTION_CHECK( runnerFunction::FlatTop            (userStruct, real, OFullSize, windowSize), "--flattop_window"           )
    
    else if(options.find("--gaussian_window=") != std::string::npos)
    {
        if(options.find("<<sigma") != std::string::npos)
        {
            auto    P1                  = options.find("--gaussian_window=") + 19;
            auto    P2                  = options.find("<<sigma");
            float   sigma               = -1.0f;
            std::string sigmaString     = options.substr(P1, P2 - P1);
            
            try {   
                sigma = std::stof(sigmaString);
            }catch( const std::exception& e){   
                return std::move("--gaussian_window");
            }
            
            if(sigma > 0)
            {
                if(!runnerFunction::Gaussian(userStruct, real, OFullSize, windowSize, sigma))
                {
                    return std::move("--gaussian_window");
                }
            }
        }
    }
    I_OPTION_CHECK(runnerFunction::RemoveDC(userStruct, real, qtConst, OFullSize, windowSize), "--remove_dc")
    
    if(windowSizeEXP < 6)
    {
        return std::move("Operation not supported");
    }
    void* realResult = real;
    void* imagResult = imag;
    switch (windowSizeEXP)
    {
    case 6:
        if(!runnerFunction::EXP6  (userStruct, real, imag, OHalfSize)){
            return std::move("Err On Stockham");
        }
        break;
    case 7:
        if(!runnerFunction::EXP7  (userStruct, real, imag, OHalfSize)){
            return std::move("Err On Stockham");
        }
        break;
    case 8:
        if(!runnerFunction::EXP8  (userStruct, real, imag, OHalfSize)){
            return std::move("Err On Stockham");
        }
        break;
    case 9:
        if(!runnerFunction::EXP9  (userStruct, real, imag, OHalfSize)){
            return std::move("Err On Stockham");
        }
        break;
    case 10:
        if(!runnerFunction::EXP10 (userStruct, real, imag, OHalfSize)){
            return std::move("Err On Stockham");
        }
        break;
    case 11:
        if(!runnerFunction::EXP11 (userStruct, real, imag, OHalfSize)){
            return std::move("Err On Stockham");
        }
        break;
    default:
        if(
        !runnerFunction::EXPC
        (
            userStruct, 
            real, 
            imag, 
            subreal, 
            subimag, 
            out, 
            (windowSize >> 1), 
            windowSizeEXP, 
            OFullSize, 
            realResult, 
            imagResult
        )){
            return std::move("Err On Stockham");
        }
        break;
    }
    
    I_OPTION_CHECK(
        runnerFunction::HalfComplex(userStruct, out, realResult, imagResult, OHalfSize, windowSizeEXP),
        "--half_complex_return"
    )else{
        runnerFunction::ToPower(userStruct, out, realResult, imagResult, OFullSize);
    }
    return std::move("OK");
}
#undef I_OPTION_CHECK
#undef EI_OPTION_CHECK
