global dma_memory_prd
global dma_memory_area


align 0x1000
dma_memory_prd:
    resb 8
    
align 0x1000
dma_memory_area:
    resb (64 * 1024)