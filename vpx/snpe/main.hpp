#include <unistd.h>


//TODO (snpe): snpe_cfg_t
//TODO (snpe): snpe_runtime_mode (CPU, GPU, GPU16, DSP)

typedef enum{
    CPU_FLOAT32,
    GPU_FLOAT32_16_HYBRID,
    DSP_FIXED8_TF,
    GPU_FLOAT16,
    AIP_FIXED8_TF
} snpe_runtime_mode;

#ifdef __cplusplus
class SNPE
{
private:
    zdl::DlSystem::Runtime_t runtime;
    std::shared_ptr<zdl::SNPE::SNPE> snpe;
public:
    SNPE(snpe_runtime_mode runtime_mode);
    ~SNPE(void);
    int check_runtime();
    int init_network(const char *);
    int execute_byte(uint8_t*,float *, int);
    int execute_float(float *,float *, int);
};
#endif

//called from libvpx
#ifdef __cplusplus
extern "C" {
#endif
    void *snpe_alloc(snpe_runtime_mode);
    void snpe_free(void *);
    int snpe_check_runtime(void *);
    int snpe_init_network(void *, const char *);
    int snpe_execute_byte(void *, uint8_t*,float *, int);
    int snpe_execute_float(void *, float *,float *, int);
#ifdef __cplusplus
}
#endif


