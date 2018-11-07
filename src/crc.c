#include "init.h"
#include "crc.h"

CRC_HandleTypeDef hcrc;

void CRC_Init() {
  __HAL_RCC_CRC_CLK_ENABLE();

  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_DISABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_DISABLE;
  hcrc.Init.GeneratingPolynomial = 0x4C11DB7;
  hcrc.Init.CRCLength = CRC_POLYLENGTH_32B;
  hcrc.Init.InitValue = 0xFFFFFFFF;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_WORDS;
  HAL_CRC_Init(&hcrc);
}
