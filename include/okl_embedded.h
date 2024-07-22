
#pragma once
//generated with python code
#include <string>
class okl_embed {
    public:
    #ifndef NO_EMBEDDED_okl
std::string STFT = 
	"#include <math.h>\n"
	"\n"
	"\n"
	"struct complex{\n"
	"    float real;\n"
	"    float imag;\n"
	"};\n"
	"\n"
	"struct pairs{\n"
	"    int first;\n"
	"    int second;\n"
	"};\n"
	"\n"
	"\n"
	"inline \n"
	"float \n"
	"window_func(const int index, const int window_size)\n"
	"{\n"
	"    float first = cos(M_PI * 2.0 * (float)index / (float)window_size);\n"
	"    float second = cos(M_PI * 4.0 * (float)index / (float)window_size);\n"
	"    return (0.42 - 0.5 * first + 0.08 * second);\n"
	"}\n"
	"\n"
	"inline int \n"
	"reverseBits(int num, int radix_2_data) {\n"
	"    int reversed = 0;\n"
	"    for (int i = 0; i < radix_2_data; ++i) {\n"
	"        reversed = (reversed << 1) | (num & 1);\n"
	"        num >>= 1;\n"
	"    }\n"
	"    return reversed;\n"
	"}\n"
	"\n"
	"inline\n"
	"pairs \n"
	"indexer(const unsigned int ID,const int powed_stage)\n"
	"{\n"
	"    pairs temp;\n"
	"    temp.first = (ID % (powed_stage)) + powed_stage * 2 * (ID / powed_stage);\n"
	"    temp.second = temp.first + powed_stage;\n"
	"    return temp;\n"
	"}\n"
	"\n"
	"complex \n"
	"twiddle(int high, int low)\n"
	"{\n"
	"    complex temp;\n"
	"    float angle = 2.0 * ((float)high / (float)low);\n"
	"    temp.real = cos(angle * M_PI);\n"
	"    temp.imag = -1.0 * sin(angle * M_PI);\n"
	"    return temp;\n"
	"}\n"
	"\n"
	"inline \n"
	"complex  \n"
	"cmult(const complex a, const complex b){\n"
	"    complex result;\n"
	"    result.real = a.real * b.real - a.imag * b.imag;\n"
	"    result.imag = a.real * b.imag + a.imag * b.real;\n"
	"\n"
	"    return result;\n"
	"}\n"
	"\n"
	"inline\n"
	"complex\n"
	"cadd(complex a, const complex b){\n"
	"    a.real += b.real;\n"
	"    a.imag += b.imag;\n"
	"    return a;\n"
	"}\n"
	"\n"
	"inline\n"
	"complex\n"
	"csub(complex a, const complex b){\n"
	"    a.real -= b.real;\n"
	"    a.imag -= b.imag;\n"
	"    return a;\n"
	"}\n"
	"\n"
	"inline \n"
	"float \n"
	"cmod(complex a){\n"
	"    return (sqrt(a.real*a.real + a.imag*a.imag));\n"
	"}\n"
	"\n"
	"\n"
	"float sums = 0;\n"
	"\n"
	"\n"
	"\n"
	"// fulsz / 0.5 =vlap\n"
	"// vlap/vsz = quot\n"
	"// vsz = win * ovratio\n"
	"// quot* win * ovratio = fulsz / ovratio\n"
	"// quot = (fullSize / overlap_ratio) / overlap_ratio / window_size\n"
	"@kernel void STFT(  float* in,\n"
	"                    float* out,\n"
	"                    complex* buffer,\n"
	"                    unsigned int fullSize,\n"
	"                    int windowSize,\n"
	"                    int quot,\n"
	"                    unsigned int OWSize,\n"
	"                    int radixData,)\n"
	"{\n"
	"    //DC remove\n"
	"    for(int once_val = 0; once_val < 1; ++once_val;@outer)\n"
	"    {\n"
	"        for(unsigned int i=0; i < fullSize;++i;@inner)\n"
	"        {\n"
	"            @atomic sums += in[i];\n"
	"            @barrier();\n"
	"            in[i] -= (sums / (float)fullSize);\n"
	"        }\n"
	"    }\n"
	"    //overlap Extend\n"
	"    for(int w_num=0; w_num < quot; ++w_num;@outer)\n"
	"    {\n"
	"        for(int w_itr=0; w_itr < windowSize; ++w_itr;@inner)\n"
	"        {\n"
	"            buffer[w_num * windowSize + w_itr].real = \n"
	"            in[w_num * OWSize + w_itr] * window_func(w_itr, windowSize);\n"
	"        }\n"
	"    }\n"
	"    //bitReverse\n"
	"    for(int w_num=0; w_num < quot; ++w_num;@outer)\n"
	"    {\n"
	"        for(int w_itr=0; w_itr < windowSize; ++w_itr;@inner)\n"
	"        {\n"
	"            float data = buffer[w_num * windowSize + w_itr].real;\n"
	"            int dst_idx = reverseBits(w_num * windowSize + w_itr, radixData);\n"
	"            @barrier();\n"
	"            buffer[dst_idx].real = data;\n"
	"        }\n"
	"    }\n"
	"    //butterfly\n"
	"    for(int stage = 0; stage < radixData; ++stage)\n"
	"    {\n"
	"        for(int w_num=0; w_num < quot; ++w_num;@outer)\n"
	"        {\n"
	"            for(int w_itr=0; w_itr < windowSize / 2; ++w_itr;@inner)\n"
	"            {\n"
	"                unsigned int GID = w_num * windowSize + w_itr;\n"
	"                int powed_stage = (int)pow(2.0, stage);\n"
	"                pairs origin_pair = indexer(GID, powed_stage);\n"
	"                complex this_twiddle = twiddle(GID % powed_stage, powed_stage * 2);\n"
	"                this_twiddle = cmult(buffer[origin_pair.second],this_twiddle);\n"
	"                complex tempx;\n"
	"                complex tempy;\n"
	"                tempx = cadd(buffer[origin_pair.first], this_twiddle);\n"
	"                tempy = csub(buffer[origin_pair.second], this_twiddle);\n"
	"                @barrier();\n"
	"                buffer[origin_pair.first] = tempx;\n"
	"                buffer[origin_pair.second] = tempy;                \n"
	"            }\n"
	"        }\n"
	"    }\n"
	"\n"
	"    //toPower\n"
	"    for(int w_num=0; w_num < quot; ++w_num;@outer)\n"
	"    {\n"
	"        for(int w_itr=0; w_itr < windowSize / 2; ++w_itr;@inner)\n"
	"        {\n"
	"            unsigned int GID = w_num * windowSize + w_itr;\n"
	"            float powered = cmod(buffer[GID]);\n"
	"            out[w_num * windowSize / 2 + w_itr] = powered;\n"
	"        }\n"
	"    }\n"
	"}\n"
	;
#endif
#ifdef NO_EMBEDDED_okl
std::string STFT = 
	"okl_kernel_files/kernel.okl\n"
	;
#endif

};