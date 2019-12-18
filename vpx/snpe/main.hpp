#include <unistd.h>


//TODO (snpe): snpe_cfg_t
//TODO (snpe): snpe_runtime_mode (CPU, GPU, GPU16, DSP)

#ifdef __cplusplus
class SNPE
{
private:
    zdl::DlSystem::Runtime_t runtime;
    std::shared_ptr<zdl::SNPE::SNPE> snpe;
public:
    SNPE(mobinas_dnn_runtime);
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
    void *snpe_alloc(mobinas_dnn_runtime);
    void snpe_free(void *);
    int snpe_check_runtime(void *);
    int snpe_init_network(void *, const char *);
    int snpe_execute_byte(void *, uint8_t*,float *, int);
    int snpe_execute_float(void *, float *,float *, int);
#ifdef __cplusplus
}
#endif


