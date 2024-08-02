#include "main.h"
#include <stdbool.h>

#define ENC_TIMER_PERIOD 65535 // エンコーダのカウント周期
#define ENC_OF_UF_THRESHOLD 32767 // オーバーフロ, アンダーフロとみなす閾値

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim14;
extern UART_HandleTypeDef huart2;

bool flag_transmit_enc_status = false;  // 1ms周期でtrueになる。trueになったらエンコーダの値をUARTで送信。
bool flag_status_reset_rquest = false;  // 中心の通過スイッチによるリセット要求があればtrue。
bool status_reset = false;              // 中心の通過スイッチでリセットされていればtrue。

// エンコーダ用の変数
int32_t enc_value_fine = 0;
int32_t enc_value_coarse = 0;
int32_t enc_value_prev_fine = 0;


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if(htim->Instance == htim14.Instance) flag_transmit_enc_status = true;
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == GPIO_PIN_3 && status_reset == false) flag_status_reset_rquest = true;
}
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == GPIO_PIN_3 && status_reset == false) flag_status_reset_rquest = true;
}

int user_main(void)
{
  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
  HAL_TIM_Base_Start_IT(&htim14);

  while (1)
  {
    if(flag_transmit_enc_status)
    {
      flag_transmit_enc_status = false;

      // エンコーダのオーバーフロー, アンダーフローを計算
      int32_t enc_diff, enc_value;
      enc_value_fine = htim1.Instance->CNT;
      enc_diff = enc_value_fine - enc_value_prev_fine;
      enc_value_prev_fine = enc_value_fine;
      if(enc_diff > ENC_OF_UF_THRESHOLD)
      {
        enc_value_coarse--;
      }
      if(enc_diff < -ENC_OF_UF_THRESHOLD)
      {
        enc_value_coarse++;
      }

      // 現在のエンコーダ値を計算
      enc_value = enc_value_coarse * ENC_TIMER_PERIOD + enc_value_fine;

      // 送信データを生成して送信
      // 送信データの例
      // リセット済み: "s" + int32_t_0 + int32_t_1 + int32_t_2 + int32_t_3
      // リセット前正: "p" + 以下同じ
      // リセット前負: "n" + 以下同じ
      uint8_t data[5] = {0};
      if(status_reset)
      {
        data[0] = 's';
      }
      else
      {
        if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET) data[0] = 'p';
        else                                                    data[0] = 'n';
      }
      data[1] = (enc_value >> 24) & 0xFF;
      data[2] = (enc_value >> 16) & 0xFF;
      data[3] = (enc_value >> 8) & 0xFF;
      data[4] = enc_value & 0xFF;

      HAL_UART_Transmit(&huart2, data, sizeof(data), HAL_MAX_DELAY);
    }

    if(flag_status_reset_rquest)
    {
      flag_status_reset_rquest = false;
      status_reset = true;
      htim1.Instance->CNT = 0;
      enc_value_fine = 0;
      enc_value_coarse = 0;
      enc_value_prev_fine = 0;
    }
  }
}
