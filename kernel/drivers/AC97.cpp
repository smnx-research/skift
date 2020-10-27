#include <libsystem/Logger.h>

#include "kernel/drivers/AC97.h"

/*

Tasks

Done
- created driver class
- initialised constants
- allocated buffer memory
- initialised driver
- set max volume
- add logging to functions
- try to initialise driver in qemu
- implement write function that starts playing

Left
- set sample rate in initialise
- handle interrupt and filebuffer memory 
- stop playing once buffer is complete or call received
- implement record using read
- implement volume control

*/

AC97::AC97(DeviceAddress address) : PCIDevice(address, DeviceClass::SOUND)
{
    // set values
    // knobs = _knobs;
    playback_format = 0;
    playback_speed = AC97_PLAYBACK_SPEED;
    playback_volume_PCM = AC97_PCM_OUT_VOLUME;
    playback_volume_master = AC97_MASTER_VOLUME;

    // get native audio bus master bar and native audio mixer bar
    nabmbar = bar(1).base();
    nambar = bar(0).base();
    lvi = 2;

    //  enable all device interrupts
    out8(nabmbar + AC97_PO_CR, AC97_X_CR_FEIE | AC97_X_CR_IOCE);

    pci_address().write16(PCI_COMMAND, 0x5);

    // default the pcm output to full volume
    out16(nambar + AC97_PCM_OUT_VOLUME, 0x0000);

    initialise_buffers();

    // tell the ac97 where buffer descriptor list is
    out32(nabmbar + AC97_PO_BDBAR, buffer_descriptors_range->physical_base());
    // set last valid index
    // lvi = 2;
    out8(nabmbar + AC97_PO_LVI, lvi);

    // detect wheter device supports MSB
    out32(nambar + AC97_MASTER_VOLUME, 0x2020);
    uint16_t t = in32(nambar + AC97_MASTER_VOLUME) & 0x1f;
    if (t == 0x1f)
    {
        // debug_print(WARNING, "This device only supports 5 bits of audio volume.");
        logger_trace("This device only supports 5 bits of audio volume.");
        bits = 5;
        mask = 0x1f;
        out32(nambar + AC97_MASTER_VOLUME, 0x0f0f);
    }
    else
    {
        bits = 6;
        mask = 0x3f;
        out32(nambar + AC97_MASTER_VOLUME, 0x1f1f);
    }

    out8(nabmbar + AC97_PO_CR, in8(nabmbar + AC97_PO_CR) | AC97_X_CR_RPBM);

    logger_trace("AC97 initialised successfully");
    // debug_print(NOTICE, "AC97 initialized successfully");

    //start device playing
}

void AC97::initialise_buffers()
{
    buffer_descriptors_range = make<MMIORange>(sizeof(AC97BufferDescriptor) * AC97_BDL_LEN);
    buffer_descriptors_list = reinterpret_cast<AC97BufferDescriptor *>(buffer_descriptors_range->base());

    for (size_t i = 0; i < AC97_BDL_LEN; i++)
    {
        buffers.push_back(make<MMIORange>(AC97_BDL_BUFFER_LEN));
        buffer_descriptors_list[i].pointer = buffers[i]->physical_base();
        AC97_CL_SET_LENGTH(buffer_descriptors_list[i].cl, AC97_BDL_BUFFER_LEN);
        /* Set all buffers to interrupt */
        // buffer_descriptors_list[i].cl |= AC97_CL_IOC;
    }
}

AC97::~AC97() { ; }

void AC97::handle_interrupt()
{
    logger_trace("interrupt received");
    uint16_t sr = in16(nabmbar + AC97_PO_SR);
    if (!sr)
    {
        return;
    }

    if (sr & AC97_X_SR_BCIS)
    {
        ;
    }
    else if (sr & AC97_X_SR_LVBCI)
    {
        logger_trace("ac97 irq is lvbci");
        playing = false;
    }
    else if (sr & AC97_X_SR_FIFOE)
    {
        logger_trace("ac97 irq is fifoe");
    }
    else
    {
        /* don't handle it */
        return;
    }
    logger_trace("ac97 status register: 0x%4x, %d", sr, sr);
    // i dont know what this does (._.')
    out16(nabmbar + AC97_PO_SR, sr & 0x1E);

    return;
}
// ResultOr<size_t> read(FsHandle &handle, void *buffer, size_t size) { ; };

bool AC97::can_write(FsHandle &handle)
{
    __unused(handle);
    return !playing;
    // return true;
}

ResultOr<size_t> AC97::write(FsHandle &handle, const void *buffer, size_t size)
{
    __unused(handle);
    // __unused(size);
    // __unused(buffer);
    // lvi = 0;

    logger_trace("in write for ac97 %d", size);
    for (int i = 0; i < 32 && size; i++)
    {
        if (size >= AC97_BDL_BUFFER_LEN)
        {
            logger_trace("inhere");
            AC97_CL_SET_LENGTH(buffer_descriptors_list[i].cl, 0xFFFE);
            size -= AC97_BDL_BUFFER_LEN;
            buffers[i]->write(0, buffer, AC97_BDL_BUFFER_LEN);
        }
        else
        {
            logger_trace("here");
            AC97_CL_SET_LENGTH(buffer_descriptors_list[i].cl, size >> 1);
            buffers[i]->write(0, buffer, size);
            size = 0;
        }
        buffer_descriptors_list[i].cl |= AC97_CL_IOC;
        if (size) // Another buffer
        {
            ;
        }
        else // no more buffer
        {
            buffer_descriptors_list[i].cl |= AC97_CL_BUP;
            lvi = i; // The last valid buffer is this one
            break;
        }
    }
    out32(nabmbar + AC97_PO_BDBAR, buffer_descriptors_range->physical_base());
    out8(nabmbar + AC97_PO_LVI, lvi);
    out8(nabmbar + AC97_PO_CR, AC97_PO_LVI);
    playing = true;
    logger_trace("out for ac97 %d %d", lvi, size);
    return SUCCESS;
}

// Result call(FsHandle &handle, IOCall request, void *args) { ; };
