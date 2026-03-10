#include "dlgeq.h"
#include "ui_dlgeq.h"
#include "settings.h"

extern Settings settings;

DlgEq::DlgEq(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DlgEq)
{
    ui->setupUi(this);

    connect(ui->checkBoxEqBypassK, SIGNAL(toggled(bool)), &settings, SLOT(setEqKBypass(bool)));

    ui->checkBoxEqBypassK->setChecked(settings.eqKBypass());

    auto eqSliderControlsK = {
        ui->verticalSliderEqK1,
        ui->verticalSliderEqK2,
        ui->verticalSliderEqK3,
        ui->verticalSliderEqK4,
        ui->verticalSliderEqK5,
        ui->verticalSliderEqK6,
        ui->verticalSliderEqK7,
        ui->verticalSliderEqK8,
        ui->verticalSliderEqK9,
        ui->verticalSliderEqK10
    };

    int band = 0;

    for (auto &slider : eqSliderControlsK)
    {
        connect(slider, &QSlider::valueChanged, [=]( int newValue ) { settings.setEqKLevel(band, newValue); });
        slider->setValue(settings.getEqKLevel(band));
        band++;
    }
}

DlgEq::~DlgEq()
{
    delete ui;
}
