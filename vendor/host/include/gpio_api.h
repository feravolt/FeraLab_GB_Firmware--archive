#ifndef _GPIO_API_H_
#define _GPIO_API_H_

A_STATUS wmi_gpio_output_set(struct wmi_t *wmip,
                             A_UINT32 set_mask,
                             A_UINT32 clear_mask,
                             A_UINT32 enable_mask,
                             A_UINT32 disable_mask);

A_STATUS wmi_gpio_input_get(struct wmi_t *wmip);

A_STATUS wmi_gpio_register_set(struct wmi_t *wmip,
                               A_UINT32 gpioreg_id,
                               A_UINT32 value);

A_STATUS wmi_gpio_register_get(struct wmi_t *wmip, A_UINT32 gpioreg_id);

A_STATUS wmi_gpio_intr_ack(struct wmi_t *wmip, A_UINT32 ack_mask);

#endif /* _GPIO_API_H_ */
