@ECHO OFF
IF /i "%1" == "new_debug" (
	@copy /y libs\app_image_bin.cfg build\gcc\app_image_bin.cfg
	make\make.exe -B -f make/gcc/gcc_makefile new C_PREDEF="-D __CUSTOMER_CODE__ -DVTS_DEBUG_LOG_ENABLE=1 -DVTS_BLE_ENABLE=0"
) ELSE (
IF /i "%1" == "new_release" (
	@copy /y libs\app_image_bin.cfg build\gcc\app_image_bin.cfg
	make\make.exe -B -f make/gcc/gcc_makefile new C_PREDEF="-D __CUSTOMER_CODE__ -DVTS_DEBUG_LOG_ENABLE=0 -DVTS_BLE_ENABLE=1"
) ELSE (
IF /i "%1" == "new_full" (
	@copy /y libs\app_image_bin.cfg build\gcc\app_image_bin.cfg
	make\make.exe -B -f make/gcc/gcc_makefile new RIL_MQTT_ENABLE=1 C_PREDEF="-D __CUSTOMER_CODE__ -DVTS_DEBUG_LOG_ENABLE=1 -DVTS_BLE_ENABLE=1 -D__OCPU_RIL_AUDIO_SUPPORT__ -D__OCPU_RIL_DTMF_SUPPORT__ -D__OCPU_RIL_QCELLLOC_SUPPORT__ -D__OCPU_RIL_ALARM_RING_SUPPORT__ -D__OCPU_RIL_VOLTAGE_URC_SUPPORT__ -D__OCPU_RIL_MQTT_SUPPORT__"
) ELSE (
IF /i "%1" == "new" (
	@copy /y libs\app_image_bin.cfg build\gcc\app_image_bin.cfg
	make\make.exe -f make/gcc/gcc_makefile 
) ELSE (
	IF /i "%1" == "clean" (
		make\make.exe %1 -f make/gcc/gcc_makefile
		IF EXIST build\gcc\build.log (
			@del /f build\gcc\build.log
		)
		IF EXIST build\gcc\app_image_bin.cfg (
			@del /f build\gcc\app_image_bin.cfg
		)
	) ELSE (
		IF /i "%1" == "help" (
			make\make.exe %1 -f make/gcc/gcc_makefile
		) ELSE (
			ECHO Incorect input argument.
		)
	)
)

)


		)
)

::make\make.exe -f make/gcc/gcc_makefile  2> build/gcc/build.log