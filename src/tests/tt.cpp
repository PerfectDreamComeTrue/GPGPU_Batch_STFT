#include <gtest/gtest.h>
#include <occa.hpp>
#include "STFT.h"
#define KERNEL_PATH "../../../include/kernel.okl"

TEST(testTemplate, tag1){
    EXPECT_EQ(1 + 32, 33);
}

TEST(TestKernel, DCRemover){
    Stft ft = Stft("serial");
    ft.addNewKernel(KERNEL_PATH, "removeDC");


    unsigned int data_length = 100;
    std::vector<float> hostmem(data_length);
    for(int i = 0; i < data_length; ++i)
    {
        hostmem[i] = (float)i;
    }
    //memory ready

    occa::memory dev_in = ft.makeMem<float>(data_length);
    dev_in.copyFrom(hostmem.data());
    ft.kern["removeDC"](dev_in, data_length);
    dev_in.copyTo(hostmem.data());
    //test
    float avg = 0;
    for(int i = 0; i < data_length; ++i)
    {
        avg += i;
    }
    avg /= (float)data_length;
    for(int i = 0; i < data_length; ++i)
    {
        EXPECT_EQ(hostmem[i], (float)i - avg);
    }
}

inline 
float 
windowFunction(const int index, const int window_size)
{
    float first = cos(M_PI * 2.0 * (float)index / (float)window_size);
    float second = cos(M_PI * 4.0 * (float)index / (float)window_size);
    return (0.42 - 0.5 * first + 0.08 * second);
}

TEST(TestKernel, OverlapNWindow){
    //ready
    Stft ft("serial");
    ft.addNewKernel(KERNEL_PATH, "overlap_N_window");

    
    //testset ready
    //(readFrame - windowSize) / OWSize-몫 + 1 = qt        OFull =readFrame - windowSize > windowSize*overlap*N 의 최소
    constexpr unsigned int readFrame = 500;
    constexpr float overlap = 0.5f;
    constexpr int radix = 6; // 32
    constexpr int windowSize = 1 << radix;
    constexpr int qt = toQuot(readFrame, overlap, windowSize);
    constexpr unsigned int OFullSize = qt * windowSize;
    constexpr unsigned int OMove = windowSize * (1.0f - overlap);

    occa::dtype_t cplx_t;
    cplx_t.registerType();
    cplx_t.addField("real", occa::dtype::float_);
    cplx_t.addField("imag", occa::dtype::float_);

    occa::memory dev_in = ft.makeMem<float>(readFrame);

    occa::memory dev_buffer = ft.makeMem(OFullSize, cplx_t);

    std::vector<float> hostMem(readFrame);
    for(int i = 0; i<readFrame;++i)
    {
        hostMem[i] = 10.0;
    }
    dev_in.copyFrom(hostMem.data());

    ft.kern["overlap_N_window"](dev_in, dev_buffer, readFrame, OFullSize, windowSize, OMove);
    
    std::vector<occacplx> hBuffer(OFullSize);
    std::vector<occacplx> Master(OFullSize);


    dev_buffer.copyTo(hBuffer.data());

    for(int i = 0; i < qt; ++i)
    {
        for(int j = 0; j < windowSize; ++j)
        {
            unsigned int read_point = i * OMove + j;
            //std::cout<<hostMem[read_point]<<std::endl;
            Master[i * windowSize + j].real =
                read_point >= readFrame ? 0.0
                :
                hostMem[read_point] * (_Float16)windowFunction(j, windowSize);
        }
    }
    for(unsigned int i = 0; i < OFullSize; ++i)
    {
        EXPECT_EQ(std::ceil(hBuffer[i].imag), std::ceil(Master[i].real));
    }
}

inline int 
reverseBits(int num, int radix_2_data) {
    int reversed = 0;
    for (int i = 0; i < radix_2_data; ++i) {
        reversed = (reversed << 1) | (num & 1);
        num >>= 1;
    }
    return reversed;
}
TEST(TestKernel, BitReverse)
{
    Stft ft("serial");
    ft.addNewKernel(KERNEL_PATH, "bitReverse");

    constexpr unsigned int readFrame = 500;
    constexpr float overlap = 0.5f;
    constexpr int radix = 6; // 32
    constexpr int windowSize = 1 << radix;
    constexpr int qt = toQuot(readFrame, overlap, windowSize);
    constexpr unsigned int OFullSize = qt * windowSize;
    constexpr unsigned int OMove = windowSize * (1.0f - overlap);

    

    occa::dtype_t cplx_t;
    cplx_t.registerType();
    cplx_t.addField("real", occa::dtype::float_);
    cplx_t.addField("imag", occa::dtype::float_);

    occa::memory dev_buffer = ft.makeMem(OFullSize, cplx_t);
    std::vector<occacplx> dbuf(OFullSize);
    std::vector<occacplx> Master(OFullSize);
    
    for(int i=0;i < qt;++i)
    {
        for(int j=0;j<windowSize;++j)
        {
            dbuf[i * windowSize + j].imag = (float)j;
            Master[i * windowSize + j].imag = (float)j;
        }
    }
    for(int i=0;i < qt;++i)
    {
        for(int j=0;j<windowSize;++j)
        {
            Master[i * windowSize + reverseBits(j, radix)].real = Master[i * windowSize + j].imag;
            std::cout<<Master[reverseBits(j, radix)].real<<", "<<j<<", "<<reverseBits(j, radix)<<std::endl;
        }
    }
    dev_buffer.copyFrom(dbuf.data());

    ft.kern["bitReverse"](dev_buffer, OFullSize, windowSize, radix);
    dev_buffer.copyTo(dbuf.data());

    for(int i=0;i < qt;++i)
    {
        for(int j=0;j<windowSize;++j)
        {
            EXPECT_EQ(dbuf[i * windowSize + j].real, Master[i * windowSize + j].real);
        }
    }
}

TEST(TestKernel, endPreProcess)
{
    Stft ft("serial");
    ft.addNewKernel(KERNEL_PATH, "endPreProcess");

    constexpr unsigned int readFrame = 500;
    constexpr float overlap = 0.5f;
    constexpr int radix = 6; // 32
    constexpr int windowSize = 1 << radix;
    constexpr int qt = toQuot(readFrame, overlap, windowSize);
    constexpr unsigned int OFullSize = qt * windowSize;
    constexpr unsigned int OMove = windowSize * (1.0f - overlap);
    
    occa::dtype_t cplx_t;
    cplx_t.registerType();
    cplx_t.addField("real", occa::dtype::float_);
    cplx_t.addField("imag", occa::dtype::float_);

    occa::memory dev_buffer = ft.makeMem(OFullSize, cplx_t);
    std::vector<occacplx> dbuf(OFullSize);
    std::vector<occacplx> Master(OFullSize);
    for(unsigned int i =0;i<OFullSize;++i)
    {
        dbuf[i].imag = 99;
        Master[i].imag = 0;
    }
    dev_buffer.copyFrom(dbuf.data());
    ft.kern["endPreProcess"](dev_buffer, OFullSize);
    dev_buffer.copyTo(dbuf.data());
    for(unsigned int i =0;i<OFullSize;++i)
    {
        EXPECT_EQ(dbuf[i].imag,Master[i].imag);
    }
}

TEST(TestKernel, Butterfly)
{
    Stft ft("serial");
    ft.addNewKernel(KERNEL_PATH, "Butterfly");

    constexpr unsigned int readFrame = 500;
    constexpr float overlap = 0.5f;
    constexpr int radix = 6; // 32
    constexpr int windowSize = 1 << radix;
    constexpr int qt = toQuot(readFrame, overlap, windowSize);
    constexpr unsigned int OFullSize = qt * windowSize;
    constexpr unsigned int OMove = windowSize * (1.0f - overlap);
    constexpr unsigned int OHalfSize = OFullSize / 2;
    occa::dtype_t cplx_t;
    cplx_t.registerType();
    cplx_t.addField("real", occa::dtype::float_);
    cplx_t.addField("imag", occa::dtype::float_);

    occa::memory dev_buffer = ft.makeMem(OFullSize, cplx_t);
    std::vector<occacplx> dbuf(OFullSize);



    
}

TEST(TestKernel, toPower)
{
    Stft ft("serial");
    ft.addNewKernel(KERNEL_PATH, "toPower");

    constexpr unsigned int readFrame = 500;
    constexpr float overlap = 0.5f;
    constexpr int radix = 6; // 32
    constexpr int windowSize = 1 << radix;
    constexpr int qt = toQuot(readFrame, overlap, windowSize);
    constexpr unsigned int OFullSize = qt * windowSize;
    constexpr unsigned int OMove = windowSize * (1.0f - overlap);
    constexpr unsigned int OHalfSize = OFullSize / 2;
    occa::dtype_t cplx_t;
    cplx_t.registerType();
    cplx_t.addField("real", occa::dtype::float_);
    cplx_t.addField("imag", occa::dtype::float_);

    occa::memory dev_buffer = ft.makeMem(OFullSize, cplx_t);
    occa::memory dev_out = ft.makeMem<float>(OHalfSize);

    std::vector<occacplx> dbuf(OFullSize);
    std::vector<float> out(OHalfSize);
    std::vector<float> Master(OHalfSize);
    for(int i=0;i < qt;++i)
    {
        for(int j=0;j<windowSize;++j)
        {
            dbuf[i * windowSize + j].imag = (float)j;
            dbuf[i * windowSize + j].real = (float)j;
        }
    }
    dev_buffer.copyFrom(dbuf.data());
    ft.kern["toPower"](dev_buffer, dev_out, OHalfSize, radix);
    dev_out.copyTo(out.data());

    for(int i=0;i < qt;++i)
    {
        constexpr int Hws = windowSize / 2;
        for(int j=0;j<Hws;++j)
        {
            Master[i * Hws + j] = sqrt(2 * (j * j));
        }
    }
    for(unsigned int i=0; i < OHalfSize;++i)
    {
        EXPECT_EQ(Master[i], out[i]);
    }

}