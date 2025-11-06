#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DRIVER_NAME "axi_iq_demodulator"
#define PHASE_100K_OFFSET 0x00
#define PHASE_110K_OFFSET 0x04
#define PHASE_120K_OFFSET 0x08

static void __iomem *base_addr;
static int major;

static ssize_t iq_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    uint32_t phase_data[3];
    if (*ppos != 0 || count < sizeof(phase_data))
        return 0;

    phase_data[0] = ioread32(base_addr + PHASE_100K_OFFSET);
    phase_data[1] = ioread32(base_addr + PHASE_110K_OFFSET);
    phase_data[2] = ioread32(base_addr + PHASE_120K_OFFSET);

    if (copy_to_user(buf, phase_data, sizeof(phase_data)))
        return -EFAULT;

    *ppos += sizeof(phase_data);
    return sizeof(phase_data);
}

static struct file_operations iq_fops = {
    .owner = THIS_MODULE,
    .read = iq_read,
};

static int iq_probe(struct platform_device *pdev) {
    struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    base_addr = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(base_addr))
        return PTR_ERR(base_addr);

    major = register_chrdev(0, DRIVER_NAME, &iq_fops);
    pr_info("IQ Demodulator driver loaded. Major: %d\n", major);
    return 0;
}

static int iq_remove(struct platform_device *pdev) {
    unregister_chrdev(major, DRIVER_NAME);
    pr_info("IQ Demodulator driver unloaded.\n");
    return 0;
}

static const struct of_device_id iq_of_match[] = {
    { .compatible = "xlnx,axi-iq-demodulator-1.0", },
    {},
};
MODULE_DEVICE_TABLE(of, iq_of_match);

static struct platform_driver iq_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = iq_of_match,
    },
    .probe = iq_probe,
    .remove = iq_remove,
};

module_platform_driver(iq_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Till Rosenband");
MODULE_DESCRIPTION("AXI4-Lite IQ Demodulator Driver");