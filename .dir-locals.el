((nil . ((eval . (set (make-local-variable 'root-path)
                      (file-name-directory
                       (let ((d (dir-locals-find-file ".")))
                         (if (stringp d) d (car d))))))
         (eval . (setq flycheck-clang-include-path
                       (list (expand-file-name "src/" root-path)
                             (expand-file-name "drivers/STM32F3xx_HAL_Driver/Inc/" root-path)
                             (expand-file-name "drivers/STM32F3xx_HAL_Driver/Inc/Legacy/" root-path)
                             (expand-file-name "drivers/CMSIS/Device/ST/STM32F3xx/Include/" root-path)
                             (expand-file-name "drivers/CMSIS/Include/" root-path)
                             (expand-file-name "middlewares/ST/STM32_USB_Device_Library/Core/Inc/" root-path)
                             (expand-file-name "middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc/" root-path))
                       flycheck-clang-definitions
                       (append flycheck-clang-definitions
                               (list "USE_HAL_DRIVER" "STM32F303xE")))))))
