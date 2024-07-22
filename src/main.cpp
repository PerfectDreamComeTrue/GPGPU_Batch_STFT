#include <iostream>

#include <occa.hpp>
#include "miniaudio.h"
#include "okl_embedded.h"
#include "STFT.h"
#include <assert.h>
#define READFRAME 1024000


ComplexVector ifft(const ComplexVector& X) {
    int N = X.size();
    if (N <= 1) {
        return X;
    }

    ComplexVector X_even(N/2), X_odd(N/2);
    for (int i = 0; i < N/2; i++) {
        X_even[i] = X[2*i];
        X_odd[i] = X[2*i + 1];
    }

    ComplexVector even_ifft = ifft(X_even);
    ComplexVector odd_ifft = ifft(X_odd);

    ComplexVector result(N);
    for (int k = 0; k < N/2; k++) {
        Complex t = std::polar(1.0, 2 * PI * k / N) * odd_ifft[k];
        result[k] = even_ifft[k] + t;
        result[k + N/2] = even_ifft[k] - t;
    }

    return result;
}

int counter =0;
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    float* data = (float*)pDevice->pUserData;
    for(int i=0;i<frameCount;++i)
    {
        if(counter < READFRAME)
        {
            ((float*)pOutput)[i]=data[counter++];

        }
    }
    // ma_decoder *ddec = (ma_decoder*)pDevice->pUserData;
    // ma_decoder_read_pcm_frames(ddec, pOutput, frameCount, NULL);
}

int main(int, char**){
    occa::device dev;
    occa::json prop = {{"mode", "rocm"}, {"platform_id", 0}, {"device_id", 0}};
    prop["verbose"] = true;
    prop["kernel/verbose"] = true;
    prop["kernel/compiler_flags"] = "-g";
    dev.setup(prop);
    
    ma_decoder_config decconf = ma_decoder_config_init(ma_format_f32, 1, 48000);
    ma_decoder dec;
    int res = ma_decoder_init_file("../../../candy.wav", &decconf, &dec);
    constexpr int readFrame = 1024*1000;
    constexpr float overlap = 0.0f;
    constexpr int windowRadix = 10;
    constexpr int windowSize = 1 << windowRadix;
    float *hostBuffer = new float[readFrame];
    ma_decoder_read_pcm_frames(&dec, hostBuffer, readFrame, NULL);
    
    occa::dtype_t complex;
    complex.registerType();
    complex.addField("real", occa::dtype::float_);
    complex.addField("imag", occa::dtype::float_);
    
    constexpr int qt = toQuot(readFrame, overlap, windowSize);
    constexpr unsigned int OFullSize = qt * windowSize;
    constexpr unsigned int OHalfSize = OFullSize / 2;
    occa::memory dev_in = dev.malloc<float>(readFrame);
    occa::memory dev_buffer = dev.malloc(OFullSize, complex);
    occa::memory dev_out = dev.malloc<float>(OHalfSize);
    dev_in.copyFrom(hostBuffer);
    //occa::stream strm = dev.createStream();
    //okl_embed ken_code;
    //occa::kernel ken = dev.buildKernelFromString(ken_code.STFT, "STFT", prop);
    occa::kernel removeDC = dev.buildKernel("../../../include/kernel.okl", "removeDC", prop);
    occa::kernel overlap_N_window = dev.buildKernel("../../../include/kernel.okl", "overlap_N_window", prop);
    occa::kernel bitReverse = dev.buildKernel("../../../include/kernel.okl", "bitReverse", prop);
    occa::kernel endPreProcess = dev.buildKernel("../../../include/kernel.okl", "endPreProcess", prop);
    occa::kernel Butterfly = dev.buildKernel("../../../include/kernel.okl", "Butterfly", prop);
    occa::kernel toPower = dev.buildKernel("../../../include/kernel.okl", "toPower", prop);
    
    
    //removeDC(dev_in, readFrame);
    overlap_N_window(dev_in, dev_buffer, readFrame, OFullSize, windowSize, windowSize / 2);
    bitReverse(dev_buffer, OFullSize, windowSize, windowRadix);
    endPreProcess(dev_buffer, OFullSize);
    
    for(int iStage=0; iStage < windowRadix; ++iStage)
    {
        Butterfly(dev_buffer, windowSize, 1 << iStage, OHalfSize, windowSize / 2, windowRadix);
        dev.finish();
    }

    occacplx* Butterout = new occacplx[OFullSize];
    dev_buffer.copyTo(Butterout);
    ComplexVector cv(READFRAME);
    auto ii = cv[0].real();
    for(int i = 0;i<READFRAME;++i)
    {
        cv[i].real(Butterout[i].real);
        cv[i].imag(Butterout[i].imag);
    }
    auto result = ifft(cv);
    std::vector<float> mus_data(READFRAME);
    for(int i = 0;i<READFRAME;++i)
    {
        mus_data[i] = cv[i].real();
    }

    ma_device_config devconf = ma_device_config_init(ma_device_type_playback);
    ma_device mdevice;
    devconf.playback.channels = 1;
    devconf.sampleRate = 48000;
    devconf.playback.format=ma_format_f32;
    devconf.dataCallback = data_callback;
    //devconf.pUserData = &dec;
    devconf.pUserData = mus_data.data();
    ma_device_init(NULL, &devconf, &mdevice);
    ma_device_start(&mdevice);

    getchar();


    return 0;
    toPower(dev_buffer, dev_out, OHalfSize, windowRadix);
    // ken(    dev_in, 
    //         dev_out,
    //         dev_buffer,
    //         readFrame,
    //         1024,
    //         qt,
    //         512,
    //         10);
    delete[] hostBuffer;
    float* output = new float[OHalfSize];
    dev_out.copyTo(output);
    
    for(int i=0;i<100;++i)//csv out
    {
        for(int j=0;j<windowSize;++j)
        {
            std::cout<<output[i*windowSize+j]<<",";
        }
        std::cout<<"0"<<std::endl;
    }
    delete[] output;
}