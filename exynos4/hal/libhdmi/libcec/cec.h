#ifndef _LINUX_CEC_H_
#define _LINUX_CEC_H_

#define CEC_IOC_MAGIC        'c'

/**
 * CEC device request code to set logical address.
 */
#define CEC_IOC_SETLADDR     _IOW(CEC_IOC_MAGIC, 0, unsigned int)

#endif /* _LINUX_CEC_H_ */
