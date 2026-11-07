/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/endian.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/smp.h>
#include <aplus/soundcard.h>
#include <aplus/vfs.h>

#include <dev/audio.h>
#include <dev/interface.h>
#include <dev/pci.h>

#include <arch/x86/cpu.h>

MODULE_NAME("audio/ac97");
MODULE_DEPS("dev/interface,dev/pci,dev/audio");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#define AC97_MAX_DEVICES 8

#define AC97_FRAGSIZE  4096
#define AC97_FRAGMENTS 32

#define AC97_DRAIN_TIMEOUT_MS 2000
#define AC97_RESET_TIMEOUT_MS 100

typedef enum {
    NAM_REG_RESET              = 0x00,
    NAM_REG_MASTER_VOLUME      = 0x02,
    NAM_REG_AUX_VOLUME         = 0x04,
    NAM_REG_MIC_VOLUME         = 0x0E,
    NAM_REG_PCM_VOLUME         = 0x18,
    NAM_REG_INPUT_DEVICE       = 0x1A,
    NAM_REG_INPUT_GAIN         = 0x1C,
    NAM_REG_MIC_GAIN           = 0x1E,
    NAM_REG_EXT_CAPABILITIES   = 0x28,
    NAM_REG_EXT_CONTROL        = 0x2A,
    NAM_REG_PCM_FRONT_DAC_RATE = 0x2C,
    NAM_REG_PCM_SURR_DAC_RATE  = 0x2E,
    NAM_REG_PCM_LFE_DAC_RATE   = 0x30,
    NAM_REG_PCM_LR_ADC_RATE    = 0x32
} ac97_nam_register_t;

typedef enum {
    NABM_PCM_IN_BASE    = 0x00,
    NABM_PCM_OUT_BASE   = 0x10,
    NABM_MIC_BASE       = 0x20,
    NABM_GLOBAL_CONTROL = 0x2C,
    NABM_GLOBAL_STATUS  = 0x30
} nabm_register_t;

typedef enum {
    NABM_REG_BDBAR = 0x00,
    NABM_REG_CIV   = 0x04,
    NABM_REG_LVI   = 0x05,
    NABM_REG_SR    = 0x06,
    NABM_REG_PICB  = 0x08,
    NABM_REG_PIV   = 0x0A,
    NABM_REG_CR    = 0x0B,
} nabm_channel_reg_t;

#define NABM_CR_RPBM  (1 << 0)
#define NABM_CR_RR    (1 << 1)
#define NABM_CR_LVBIE (1 << 2)
#define NABM_CR_FEIE  (1 << 3)
#define NABM_CR_IOCE  (1 << 4)

#define NABM_SR_DCH   (1 << 0)
#define NABM_SR_CELV  (1 << 1)
#define NABM_SR_LVBCI (1 << 2)
#define NABM_SR_BCIS  (1 << 3)
#define NABM_SR_FIFOE (1 << 4)

#define NABM_GC_GIE (1 << 0)

#define NAM_EXT_VRA (1 << 0)

#define AC97_BD_IOC (1 << 15)
#define AC97_BD_BUP (1 << 14)

#define AC97_VOLUME_MUTE 0x8000

static struct {
    uint16_t vid;
    uint16_t did;
} pci_ids[] = {
    {0x8086, 0x2415},
};

struct ac97_bd {
    uint32_t addr;
    uint16_t samples;
    uint16_t flags;
} __packed;

struct ac97 {
    _Atomic(bool) ready;
    uintptr_t nam;
    uintptr_t nabm;

    pcidev_t pci;
    irq_t irq;

    device_t dsp;
    device_t mixer;

    struct ac97_bd* bdl;
    uint32_t bdl_phys;

    uint8_t* buffer;
    uint32_t buffer_phys;

    size_t fragsize;
    size_t nfrags;

    uint8_t w_frag;
    size_t w_off;
    uint8_t lvi;
    bool running;

    bool vra;

    uint32_t levels[SOUND_MIXER_NRDEVICES];
};

static struct ac97 ac97_devices[AC97_MAX_DEVICES] = {0};
static uint16_t num_devices                       = 0;


static int __ac97_wait_warm_reset(struct ac97* sc) {
    uint64_t now = arch_timer_generic_getms();
    while ((inl(sc->nabm + NABM_GLOBAL_CONTROL) & (1 << 2))) {
        if (arch_timer_generic_getms() - now > 10) {
            return -ETIMEDOUT;
        }
        __cpu_pause();
    }
    return 0;
}

static int __ac97_wait_codec_ready(struct ac97* sc) {
    uint64_t now = arch_timer_generic_getms();
    while ((inl(sc->nabm + NABM_GLOBAL_STATUS) & (1 << 8)) == 0) {
        if (arch_timer_generic_getms() - now > 1000) {
            return -ETIMEDOUT;
        }
        __cpu_pause();
    }
    return 0;
}


static uint16_t __ac97_volume(uint32_t level, uint8_t unity, uint8_t attenuation) {

    uint32_t l = AUDIO_MIXER_LEFT(level);
    uint32_t r = AUDIO_MIXER_RIGHT(level);

    if (l == 0 && r == 0) {
        return AC97_VOLUME_MUTE;
    }

    if (l > 100) {
        l = 100;
    }

    if (r > 100) {
        r = 100;
    }

    uint16_t la = (uint16_t)(unity + ((100 - l) * (attenuation - unity)) / 100);
    uint16_t ra = (uint16_t)(unity + ((100 - r) * (attenuation - unity)) / 100);

    return (uint16_t)((la << 8) | ra);
}


static void __ac97_mixer_apply(struct ac97* sc, int dev) {

    switch (dev) {

        case SOUND_MIXER_VOLUME:
            outw(sc->nam + NAM_REG_MASTER_VOLUME, __ac97_volume(sc->levels[dev], 0x00, 0x3F));
            break;

        case SOUND_MIXER_PCM:
            outw(sc->nam + NAM_REG_PCM_VOLUME, __ac97_volume(sc->levels[dev], 0x08, 0x1F));
            break;

        case SOUND_MIXER_MIC:
            outw(sc->nam + NAM_REG_MIC_VOLUME, __ac97_volume(sc->levels[dev], 0x08, 0x1F));
            break;

        default:
            break;
    }
}


static void __ac97_mixer_restore(struct ac97* sc) {

    __ac97_mixer_apply(sc, SOUND_MIXER_VOLUME);
    __ac97_mixer_apply(sc, SOUND_MIXER_PCM);
    __ac97_mixer_apply(sc, SOUND_MIXER_MIC);
}


static void __ac97_stop(struct ac97* sc) {

    outb(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_CR, 0);

    sc->running = false;
}


static void __ac97_reset_dma(struct ac97* sc) {

    __ac97_stop(sc);

    outb(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_CR, NABM_CR_RR);

    uint64_t now = arch_timer_generic_getms();

    while (inb(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_CR) & NABM_CR_RR) {

        if (arch_timer_generic_getms() - now > AC97_RESET_TIMEOUT_MS) {
            break;
        }

        __cpu_pause();
    }

    outw(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_SR, NABM_SR_LVBCI | NABM_SR_BCIS | NABM_SR_FIFOE);
    outl(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_BDBAR, sc->bdl_phys);
    outb(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_LVI, 0);

    sc->w_frag = 0;
    sc->w_off  = 0;
    sc->lvi    = 0;
}


static size_t __ac97_free_frags(struct ac97* sc) {

    size_t used = sc->w_off ? 1 : 0;

    if (sc->running) {

        uint8_t civ = inb(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_CIV);

        used += (size_t)((uint8_t)(sc->lvi - civ) & (sc->nfrags - 1)) + 1;
    }

    if (used >= sc->nfrags) {
        return 0;
    }

    return sc->nfrags - used;
}


static void __ac97_publish(struct ac97* sc) {

    sc->lvi    = sc->w_frag;
    sc->w_frag = (uint8_t)((sc->w_frag + 1) & (sc->nfrags - 1));

    outb(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_LVI, sc->lvi);

    if (!sc->running) {

        outb(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_CR, NABM_CR_RPBM | NABM_CR_IOCE | NABM_CR_LVBIE | NABM_CR_FEIE);

        sc->running = true;
    }
}


static int ac97_configure(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct ac97* sc = device->userdata;

    if (unlikely(!atomic_load(&sc->ready))) {
        return -EIO;
    }


    device->snd.params.format   = AFMT_S16_LE;
    device->snd.params.channels = 2;

    uint32_t rate = device->snd.params.rate;


    scoped_lock(&device->lock) {

        __ac97_reset_dma(sc);

        if (sc->vra) {

            outw(sc->nam + NAM_REG_PCM_FRONT_DAC_RATE, (uint16_t)rate);

            rate = inw(sc->nam + NAM_REG_PCM_FRONT_DAC_RATE);

        } else {

            rate = 48000;
        }

        __ac97_mixer_restore(sc);
    }

    device->snd.params.rate = rate ? rate : 48000;

    return 0;
}


static ssize_t ac97_output(device_t* device, const void* buf, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(buf);

    struct ac97* sc = device->userdata;

    if (unlikely(!atomic_load(&sc->ready))) {
        return -EIO;
    }


    size_t done = 0;

    scoped_lock(&device->lock) {

        while (done < size) {

            if (sc->w_off == 0 && __ac97_free_frags(sc) == 0) {
                break;
            }


            size_t n = MIN(sc->fragsize - sc->w_off, size - done);

            memcpy(&sc->buffer[sc->w_frag * sc->fragsize + sc->w_off], (const uint8_t*)buf + done, n);

            done += n;
            sc->w_off += n;

            if (sc->w_off == sc->fragsize) {

                sc->w_off = 0;

                __ac97_publish(sc);
            }
        }
    }


    if (!done) {
        return -EAGAIN;
    }

    return (ssize_t)done;
}


static ssize_t ac97_ospace(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct ac97* sc = device->userdata;

    ssize_t space = 0;

    scoped_lock(&device->lock) {

        space = (ssize_t)(__ac97_free_frags(sc) * sc->fragsize);

        if (sc->w_off) {
            space += (ssize_t)(sc->fragsize - sc->w_off);
        }
    }

    return space;
}


static ssize_t ac97_odelay(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct ac97* sc = device->userdata;

    ssize_t delay = 0;

    scoped_lock(&device->lock) {

        delay = (ssize_t)sc->w_off;

        if (sc->running) {

            uint8_t civ   = inb(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_CIV);
            uint16_t picb = inw(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_PICB);

            size_t queued = (size_t)((uint8_t)(sc->lvi - civ) & (sc->nfrags - 1));

            delay += (ssize_t)(queued * sc->fragsize + (size_t)picb * 2);
        }
    }

    return delay;
}


static int ac97_trigger(device_t* device, int mask) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct ac97* sc = device->userdata;

    scoped_lock(&device->lock) {

        if (!(mask & PCM_ENABLE_OUTPUT)) {

            __ac97_stop(sc);

        } else if (!sc->running && sc->lvi != sc->w_frag) {

            outb(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_CR, NABM_CR_RPBM | NABM_CR_IOCE | NABM_CR_LVBIE | NABM_CR_FEIE);

            sc->running = true;
        }
    }

    return 0;
}


static int ac97_flush(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct ac97* sc = device->userdata;

    scoped_lock(&device->lock) {
        __ac97_reset_dma(sc);
    }

    return 0;
}


static int ac97_drain(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct ac97* sc = device->userdata;


    scoped_lock(&device->lock) {

        if (sc->w_off) {

            memset(&sc->buffer[sc->w_frag * sc->fragsize + sc->w_off], 0, sc->fragsize - sc->w_off);

            sc->w_off = 0;

            __ac97_publish(sc);
        }
    }


    uint64_t now = arch_timer_generic_getms();

    while (sc->running) {

        uint16_t sr = inw(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_SR);
        uint8_t civ = inb(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_CIV);

        if ((sr & NABM_SR_DCH) && civ == sc->lvi) {
            break;
        }

        if (arch_timer_generic_getms() - now > AC97_DRAIN_TIMEOUT_MS) {
            break;
        }

        __cpu_pause();
    }


    scoped_lock(&device->lock) {
        __ac97_reset_dma(sc);
    }

    return 0;
}


static int ac97_mixer_get(device_t* device, int dev, uint32_t* value) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(value);

    struct ac97* sc = device->userdata;

    switch (dev) {

        case SOUND_MIXER_VOLUME:
        case SOUND_MIXER_PCM:
        case SOUND_MIXER_MIC:

            *value = sc->levels[dev];
            return 0;

        default:
            break;
    }

    return -EINVAL;
}


static int ac97_mixer_set(device_t* device, int dev, uint32_t* value) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(value);

    struct ac97* sc = device->userdata;

    switch (dev) {

        case SOUND_MIXER_VOLUME:
        case SOUND_MIXER_PCM:
        case SOUND_MIXER_MIC:
            break;

        default:
            return -EINVAL;
    }

    scoped_lock(&device->lock) {

        sc->levels[dev] = *value;

        __ac97_mixer_apply(sc, dev);
    }

    return 0;
}


static void ac97_irq(pcidev_t pcidev, irq_t irq, pci_irq_data_t data, uint16_t index) {

    (void)pcidev;
    (void)irq;
    (void)index;

    struct ac97* sc = (struct ac97*)data;

    DEBUG_ASSERT(sc);


    uint16_t sr = inw(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_SR);

    if (!(sr & (NABM_SR_LVBCI | NABM_SR_BCIS | NABM_SR_FIFOE))) {
        return;
    }

    outw(sc->nabm + NABM_PCM_OUT_BASE + NABM_REG_SR, sr & (NABM_SR_LVBCI | NABM_SR_BCIS | NABM_SR_FIFOE));

    audio_notify(&sc->dsp);
}


static void ac97_reset(device_t* device) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct ac97* sc = device->userdata;

    atomic_store(&sc->ready, false);
    {
        uint32_t cnt = inl(sc->nabm + NABM_GLOBAL_CONTROL) & ~(1 << 0);
        if (cnt & (1 << 1)) {

            outl(sc->nabm + NABM_GLOBAL_CONTROL, cnt | (1 << 2));
            if (__ac97_wait_warm_reset(sc) < 0) {
#if DEBUG_LEVEL_ERROR
                kprintf("ac97: ERROR! failed to warm reset, timeout reached\n");
#endif
                device->status = DEVICE_STATUS_FAILED;
                return;
            }

        } else {
            outl(sc->nabm + NABM_GLOBAL_CONTROL, cnt | (1 << 1));
        }

        if (__ac97_wait_codec_ready(sc) < 0) {
#if DEBUG_LEVEL_ERROR
            kprintf("ac97: ERROR! failed to cold reset, timeout reached\n");
#endif
            device->status = DEVICE_STATUS_FAILED;
            return;
        }

        outw(sc->nam + NAM_REG_RESET, 0);

        if (__ac97_wait_codec_ready(sc) < 0) {
#if DEBUG_LEVEL_ERROR
            kprintf("ac97: ERROR! failed to nam reset, timeout reached\n");
#endif
            device->status = DEVICE_STATUS_FAILED;
            return;
        }

        outw(sc->nam + NAM_REG_AUX_VOLUME, 0x0000);
        outw(sc->nam + NAM_REG_PCM_VOLUME, 0x0808);
        outw(sc->nam + NAM_REG_MASTER_VOLUME, 0x0000);

        sc->vra = !!(inw(sc->nam + NAM_REG_EXT_CAPABILITIES) & NAM_EXT_VRA);

        if (sc->vra) {
            outw(sc->nam + NAM_REG_EXT_CONTROL, inw(sc->nam + NAM_REG_EXT_CONTROL) | NAM_EXT_VRA);
        }

        outl(sc->nabm + NABM_GLOBAL_CONTROL, inl(sc->nabm + NABM_GLOBAL_CONTROL) | NABM_GC_GIE);

        __ac97_mixer_restore(sc);
        __ac97_reset_dma(sc);
    }
    atomic_store(&sc->ready, true);
}


static void ac97_init(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct ac97* sc = device->userdata;


    sc->fragsize = AC97_FRAGSIZE;
    sc->nfrags   = AC97_FRAGMENTS;

    sc->levels[SOUND_MIXER_VOLUME] = AUDIO_MIXER_LEVEL(100, 100);
    sc->levels[SOUND_MIXER_PCM]    = AUDIO_MIXER_LEVEL(100, 100);
    sc->levels[SOUND_MIXER_MIC]    = AUDIO_MIXER_LEVEL(50, 50);


    uintptr_t bdl = pmm_alloc_block();

    if (unlikely(bdl == PMM_INVALID_ADDRESS)) {
        kpanicf("ac97: PANIC! failed to allocate the buffer descriptor list\n");
    }

    uintptr_t buffer = pmm_alloc_blocks((sc->fragsize * sc->nfrags) / PML1_PAGESIZE);

    if (unlikely(buffer == PMM_INVALID_ADDRESS)) {
        kpanicf("ac97: PANIC! failed to allocate the pcm buffer\n");
    }

    if (unlikely(bdl > UINT32_MAX || (buffer + sc->fragsize * sc->nfrags) > UINT32_MAX)) {
        kpanicf("ac97: PANIC! dma memory is not addressable with 32 bits: bdl(%p) buffer(%p)\n", bdl, buffer);
    }


    sc->bdl_phys    = (uint32_t)bdl;
    sc->buffer_phys = (uint32_t)buffer;

    sc->bdl    = (struct ac97_bd*)arch_vmm_p2v(bdl, ARCH_VMM_AREA_HEAP);
    sc->buffer = (uint8_t*)arch_vmm_p2v(buffer, ARCH_VMM_AREA_HEAP);

    memset(sc->buffer, 0, sc->fragsize * sc->nfrags);

    for (size_t i = 0; i < sc->nfrags; i++) {

        sc->bdl[i].addr    = (uint32_t)(sc->buffer_phys + i * sc->fragsize);
        sc->bdl[i].samples = (uint16_t)(sc->fragsize / sizeof(uint16_t));
        sc->bdl[i].flags   = AC97_BD_IOC;
    }


    ac97_reset(device);

    if (device->status == DEVICE_STATUS_FAILED) {
        return;
    }


    if (sc->irq != PCI_INTERRUPT_LINE_NONE) {

        pci_intx_map_irq(sc->pci, sc->irq, (pci_irq_handler_t)ac97_irq, (pci_irq_data_t)sc);
        pci_intx_unmask(sc->pci);
    }
}


static void ac97_dnit(device_t* device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct ac97* sc = device->userdata;

    scoped_lock(&device->lock) {
        __ac97_reset_dma(sc);
    }

    if (sc->irq != PCI_INTERRUPT_LINE_NONE) {

        pci_intx_mask(sc->pci);
        pci_intx_unmap_irq(sc->pci);
    }

    atomic_store(&sc->ready, false);
}


static void pci_find(pcidev_t pcidev, uint16_t vid, uint16_t did, void* arg) {

    bool found = false;
    for (size_t i = 0; i < sizeof(pci_ids) / sizeof(pci_ids[0]); i++) {
        if (vid == pci_ids[i].vid && did == pci_ids[i].did) {
            found = true;
            break;
        }
    }

    if (!found) {
        return;
    }

    pci_enable_bus_mastering(pcidev);
    pci_enable_pio(pcidev);


    struct ac97* sc = &ac97_devices[num_devices++];

    sc->nam = pci_read(pcidev, PCI_BAR0, 4) & PCI_BAR_IO_MASK;
    if (unlikely(!sc->nam)) {
#if DEBUG_LEVEL_ERROR
        kprintf("ac97: failed to map NAM for device(%d) index(%d)\n", pcidev, num_devices - 1);
#endif
        return;
    }

    sc->nabm = pci_read(pcidev, PCI_BAR1, 4) & PCI_BAR_IO_MASK;
    if (unlikely(!sc->nabm)) {
#if DEBUG_LEVEL_ERROR
        kprintf("ac97: failed to map NAB for device(%d) index(%d)\n", pcidev, num_devices - 1);
#endif
        return;
    }

    sc->pci = pcidev;
    sc->irq = (irq_t)pci_read(pcidev, PCI_INTERRUPT_LINE, 1);
}

void init(void) {

    if (strstr(core->boot.cmdline, "audio=off"))
        return;

    pci_scan(&pci_find, PCI_TYPE_AUDIO, NULL);

    for (size_t i = 0; i < num_devices; i++) {

        struct ac97* sc = &ac97_devices[i];

        if (unlikely(!sc->nam || !sc->nabm)) {
            continue;
        }


        strcpy(sc->dsp.description, "AC97 Audio Controller");

        sc->dsp.type     = DEVICE_TYPE_AUDIO;
        sc->dsp.status   = DEVICE_STATUS_UNKNOWN;
        sc->dsp.iobase   = sc->nabm;
        sc->dsp.userdata = sc;

        sc->dsp.init  = ac97_init;
        sc->dsp.dnit  = ac97_dnit;
        sc->dsp.reset = ac97_reset;

        sc->dsp.snd.formats     = AFMT_S16_LE;
        sc->dsp.snd.minrate     = 8000;
        sc->dsp.snd.maxrate     = 48000;
        sc->dsp.snd.maxchannels = 2;
        sc->dsp.snd.fragsize    = AC97_FRAGSIZE;
        sc->dsp.snd.fragcount   = AC97_FRAGMENTS;

        sc->dsp.snd.configure = ac97_configure;
        sc->dsp.snd.output    = ac97_output;
        sc->dsp.snd.ospace    = ac97_ospace;
        sc->dsp.snd.odelay    = ac97_odelay;
        sc->dsp.snd.trigger   = ac97_trigger;
        sc->dsp.snd.drain     = ac97_drain;
        sc->dsp.snd.flush     = ac97_flush;

        device_mkdev(&sc->dsp, 0666);


        sc->mixer.type     = DEVICE_TYPE_MIXER;
        sc->mixer.status   = DEVICE_STATUS_UNKNOWN;
        sc->mixer.iobase   = sc->nam;
        sc->mixer.userdata = sc;

        sc->mixer.mix.card       = &sc->dsp;
        sc->mixer.mix.devmask    = SOUND_MASK_VOLUME | SOUND_MASK_PCM | SOUND_MASK_MIC;
        sc->mixer.mix.stereomask = SOUND_MASK_VOLUME | SOUND_MASK_PCM;
        sc->mixer.mix.get        = ac97_mixer_get;
        sc->mixer.mix.set        = ac97_mixer_set;

        device_mkdev(&sc->mixer, 0666);
    }
}

void dnit(void) {
}
