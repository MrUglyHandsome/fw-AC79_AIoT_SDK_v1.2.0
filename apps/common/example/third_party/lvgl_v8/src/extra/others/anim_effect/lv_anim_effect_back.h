#ifndef LV_ANIM_EFFECT_BACK_H
#define LV_ANIM_EFFECT_BACK_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "lv_anim_effect_global.h"
#if LV_USE_ANIM_EFFECT

void lv_anim_effect_back_in_down(lv_obj_t *obj, lv_anim_effect_args_t *anim_args);
void lv_anim_effect_back_in_left(lv_obj_t *obj, lv_anim_effect_args_t *anim_args);
void lv_anim_effect_back_in_right(lv_obj_t *obj, lv_anim_effect_args_t *anim_args);
void lv_anim_effect_back_in_up(lv_obj_t *obj, lv_anim_effect_args_t *anim_args);

void lv_anim_effect_back_out_down(lv_obj_t *obj, lv_anim_effect_args_t *anim_args);
void lv_anim_effect_back_out_left(lv_obj_t *obj, lv_anim_effect_args_t *anim_args);
void lv_anim_effect_back_out_right(lv_obj_t *obj, lv_anim_effect_args_t *anim_args);
void lv_anim_effect_back_out_up(lv_obj_t *obj, lv_anim_effect_args_t *anim_args);

#endif /*LV_USE_ANIM_EFFECT*/
#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif