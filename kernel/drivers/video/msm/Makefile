obj-y := msm_fb.o
obj-$(CONFIG_FB_MSM_LOGO) += logo.o
obj-$(CONFIG_FB_BACKLIGHT) += msm_fb_bl.o
obj-y += mdp.o
obj-y += mdp_hw_init.o
obj-y += mdp_ppp.o
ifeq ($(CONFIG_FB_MSM_MDP31),y)
obj-y += mdp_ppp_v31.o
endif
obj-y += mdp_dma_lcdc.o
obj-y += mdp_dma.o
obj-y += mdp_dma_s.o
obj-y += mdp_vsync.o
obj-y += mdp_cursor.o
obj-y += mdp_dma_tv.o
msm_mddi-objs := mddi.o mddihost.o mddihosti.o
obj-$(CONFIG_FB_MSM_MDDI) += msm_mddi.o
obj-y += msm_fb_panel.o
obj-$(CONFIG_FB_MSM_MDDI_TMD_NT35580) += mddi_tmd_nt35580.o

clean:
	rm *.o .*cmd
