# TIR-39 — consumo do ADAU1787 e fonte de DVDD

## Objetivo

Medir, na EVAL-ADAU1787Z, quanto o estado do ADAU1787 e a fonte de DVDD
contribuem para o consumo observado no Tiresias. O ensaio deve separar:

1. hardware power-down;
2. software power-down;
3. domínio digital mínimo ligado;
4. DVDD gerado pelo regulador interno do ADAU1787;
5. DVDD externo de 0,9 V.

O nRF5340 Audio DK funciona somente como controlador I2C e de `!PD`. Ele é
alimentado separadamente e não faz parte da medição de corrente da EVAL.

## Estados requeridos

### Hardware power-down

- `!PD=0` durante toda a medida;
- nenhum acesso I2C após entrar nesse estado;
- os registradores internos não são considerados acessíveis;
- com DVDD interno, espera-se que DVDD caia para aproximadamente 0 V.

Este é o menor estado de consumo do ADAU1787 e serve como baseline.

### Software power-down

- liberar `!PD` para nível alto;
- inicializar o ADAU1787 e manter o control port acessível;
- desligar ADCs, DACs, headphone, PLL, microphone bias, PGAs, DMIC, portas
  seriais, DSPs, ASRC, interpoladores, decimadores e keep-alives;
- escrever `CHIP_PWR=0x14`, portanto `POWER_EN=0`.

Este estado mede o custo de manter o componente fora do reset físico, mas com
os domínios funcionais desligados por software.

### Digital mínimo ligado

- mesma configuração de blocos desligados usada no software power-down;
- escrever `CHIP_PWR=0x15`, portanto `POWER_EN=1`;
- manter PLL, DSPs, ADCs, DACs, SAI e demais blocos funcionais desligados.

Este estado mede o custo incremental do domínio digital mínimo necessário para
manter o ADAU1787 inicializado e acessível pelo control port. Ele não é uma
aplicação de áudio e não deve processar ou transmitir sinal.

## Controles no Audio DK

| Botão | Estado aplicado | `CHIP_PWR` final |
|---|---|---:|
| `VOL-` / Button 1 | Software power-down | `0x14` |
| `VOL+` / Button 2 | Digital mínimo ligado | `0x15` |

Ao iniciar, o firmware deve manter `!PD=0` para permitir a medida de hardware
power-down antes que qualquer botão seja pressionado.

Para cada estado comandado por botão, o firmware deve:

1. executar um ciclo determinístico de `!PD`;
2. inicializar o ADAU1787 com o export SigmaStudio vazio deste perfil;
3. aplicar o estado final solicitado;
4. ler de volta e registrar os registradores de potência relevantes;
5. emitir uma mensagem inequívoca de estado pronto para medição.

O export vazio não contém fluxo de áudio, programa FastDSP ou parâmetros de
processamento. O projeto está em
`profiles/eval-i2c/sigma/tiresias-tir39-empty/tiresias-tir39-empty.dspproj`.

## Ligações

| Audio DK | nRF5340 | EVAL-ADAU1787Z |
|---|---|---|
| `D9` | `P1.13` | `SDA` |
| `D10` | `P1.12` | `SCL` |
| `D5` | `P1.14` | rede `!PD`, com `J15` aberto |
| `GND` | — | `GND` |

Usar resistores série de 330 Ω a 470 Ω em SDA, SCL e `!PD`. Não interligar as
saídas de 1,8 V das duas placas. Os pull-ups do I2C devem vir apenas da EVAL e
estar referenciados ao IOVDD dela.

Configuração da EVAL:

- `S4=HIGH` e `S1=HIGH`: endereço I2C de 7 bits `0x2B`;
- `S2=OFF`: self-boot desabilitado;
- `J25`: posição I2C;
- `J15`: aberto; D5 ligado ao lado com continuidade até `!PD` do ADAU1787.

## Configurações de DVDD

| Configuração | Jumpers e alimentação |
|---|---|
| DVDD interno | `J12` aberto e `J24` em ON |
| DVDD externo | EVAL desligada; `J24` em OFF, `J12` fechado, `JP1` em EXT e fonte de 0,9 V limitada em corrente conectada a `J3` |

Na configuração externa, registrar também a corrente da fonte de 0,9 V. A
comparação deve usar a potência total: potência de entrada da EVAL mais potência
fornecida externamente a DVDD.

## Matriz de medidas

Alimentar a EVAL com 4,2 V pelo Power Profiler. Em cada ponto, registrar três
leituras estabilizadas de corrente e as tensões DVDD, AVDD e IOVDD.

| ID | Estado do ADAU1787 | DVDD |
|---|---|---|
| I1 | Hardware power-down | Interno |
| I2 | Software power-down | Interno |
| I3 | Digital mínimo ligado | Interno |
| E1 | Hardware power-down | Externo |
| E2 | Software power-down | Externo |
| E3 | Digital mínimo ligado | Externo |

O resultado deve permitir calcular separadamente:

- custo de liberar `!PD`: I2 − I1 e E2 − E1;
- custo de `POWER_EN=1`: I3 − I2 e E3 − E2;
- diferença entre DVDD interno e externo em estados equivalentes.

## Evidência exigida no RTT

Uma medida I2, I3, E2 ou E3 só é válida se o log confirmar:

- comunicação com o ADAU1787 no endereço `0x2B`;
- identidade `41 17 87 xx`;
- conclusão do download vazio;
- valor final de `CHIP_PWR`;
- readback dos registradores de potência.

## Estado atual

O controlador `i2c@b000` é inicializado, mas a primeira escrita em `0x2B`
retorna `-5` (`EIO`) após a liberação de `!PD`. Isso ocorre tanto com `VOL-`
quanto com `VOL+`, antes do download SigmaStudio. Portanto, o firmware atual
ainda não produz medidas I2, I3, E2 ou E3 válidas.

## Build

```sh
west build -p always -b nrf5340_audio_dk/nrf5340/cpuapp --sysbuild . \
  -d build_tir39_eval_i2c -- \
  -DCONF_FILE=profiles/eval-i2c/app.conf \
  -DSB_CONF_FILE=profiles/eval-i2c/sysbuild.conf \
  -DEXTRA_DTC_OVERLAY_FILE=profiles/eval-i2c/eval-i2c.overlay
```

Saída esperada: `build_tir39_eval_i2c/merged.hex`. O perfil não deve iniciar o
network core, Bluetooth, I2S ou qualquer carga de áudio.
