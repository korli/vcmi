/*
 * firstlaunch_moc.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "firstlaunch_moc.h"
#include "ui_firstlaunch_moc.h"

#include "mainwindow_moc.h"
#include "modManager/cmodlistview_moc.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/CGeneralTextHandler.h"
#include "../../lib/Languages.h"
#include "../../lib/VCMIDirs.h"
#include "../../lib/filesystem/Filesystem.h"
#include "../languages.h"

FirstLaunchView::FirstLaunchView(QWidget * parent)
	: QWidget(parent)
	, ui(new Ui::FirstLaunchView)
{
	ui->setupUi(this);

	enterSetup();
	activateTabLanguage();

	ui->lineEditDataSystem->setText(pathToQString(boost::filesystem::absolute(VCMIDirs::get().dataPaths().front())));
	ui->lineEditDataUser->setText(pathToQString(boost::filesystem::absolute(VCMIDirs::get().userDataPath())));
}

void FirstLaunchView::on_buttonTabLanguage_clicked()
{
	activateTabLanguage();
}

void FirstLaunchView::on_buttonTabHeroesData_clicked()
{
	activateTabHeroesData();
}

void FirstLaunchView::on_buttonTabModPreset_clicked()
{
	activateTabModPreset();
}

void FirstLaunchView::on_listWidgetLanguage_currentRowChanged(int currentRow)
{
	languageSelected(ui->listWidgetLanguage->item(currentRow)->data(Qt::UserRole).toString());
}

void FirstLaunchView::changeEvent(QEvent * event)
{
	if(event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		Languages::fillLanguages(ui->listWidgetLanguage, false);
	}
	QWidget::changeEvent(event);
}

void FirstLaunchView::on_pushButtonLanguageNext_clicked()
{
	activateTabHeroesData();
}

void FirstLaunchView::on_pushButtonDataNext_clicked()
{
	activateTabModPreset();
}

void FirstLaunchView::on_pushButtonDataBack_clicked()
{
	activateTabLanguage();
}

void FirstLaunchView::on_pushButtonDataSearch_clicked()
{
	heroesDataUpdate();
}

void FirstLaunchView::on_pushButtonDataCopy_clicked()
{
	copyHeroesData();
}

void FirstLaunchView::on_pushButtonDataHelp_clicked()
{
	static const QUrl vcmibuilderWiki("https://wiki.vcmi.eu/Installation_on_Linux#Installing_Heroes_III_data_files");
	QDesktopServices::openUrl(vcmibuilderWiki);
}

void FirstLaunchView::on_comboBoxLanguage_currentIndexChanged(int index)
{
	forceHeroesLanguage(ui->comboBoxLanguage->itemData(index).toString());
}

void FirstLaunchView::enterSetup()
{
	Languages::fillLanguages(ui->listWidgetLanguage, false);
}

void FirstLaunchView::setSetupProgress(int progress)
{
	int value = std::max(progress, ui->setupProgressBar->value());

	ui->setupProgressBar->setValue(value);

	ui->buttonTabLanguage->setDisabled(value < 1);
	ui->buttonTabHeroesData->setDisabled(value < 2);
	ui->buttonTabModPreset->setDisabled(value < 3);
}

void FirstLaunchView::activateTabLanguage()
{
	setSetupProgress(1);
	ui->installerTabs->setCurrentIndex(0);
	ui->buttonTabLanguage->setChecked(true);
	ui->buttonTabHeroesData->setChecked(false);
	ui->buttonTabModPreset->setChecked(false);
}

void FirstLaunchView::activateTabHeroesData()
{
	setSetupProgress(2);
	ui->installerTabs->setCurrentIndex(1);
	ui->buttonTabLanguage->setChecked(false);
	ui->buttonTabHeroesData->setChecked(true);
	ui->buttonTabModPreset->setChecked(false);

	if(!hasVCMIBuilderScript)
	{
		ui->pushButtonDataHelp->hide();
		ui->labelDataHelp->hide();
	}
	heroesDataUpdate();
}

void FirstLaunchView::activateTabModPreset()
{
	setSetupProgress(3);
	ui->installerTabs->setCurrentIndex(2);
	ui->buttonTabLanguage->setChecked(false);
	ui->buttonTabHeroesData->setChecked(false);
	ui->buttonTabModPreset->setChecked(true);

	modPresetUpdate();
}

void FirstLaunchView::exitSetup()
{
	if(auto * mainWindow = dynamic_cast<MainWindow *>(qApp->activeWindow()))
		mainWindow->exitSetup();
}

// Tab Language
void FirstLaunchView::languageSelected(const QString & selectedLanguage)
{
	Settings node = settings.write["general"]["language"];
	node->String() = selectedLanguage.toStdString();

	if(auto * mainWindow = dynamic_cast<MainWindow *>(qApp->activeWindow()))
		mainWindow->updateTranslation();
}

void FirstLaunchView::heroesDataUpdate()
{
	if(heroesDataDetect())
		heroesDataDetected();
	else
		heroesDataMissing();
}

void FirstLaunchView::heroesDataMissing()
{
	QPalette newPalette = palette();
	newPalette.setColor(QPalette::Base, QColor(200, 50, 50));
	ui->lineEditDataSystem->setPalette(newPalette);
	ui->lineEditDataUser->setPalette(newPalette);

	ui->pushButtonDataSearch->setVisible(true);
	ui->pushButtonDataCopy->setVisible(true);

	ui->labelDataSearch->setVisible(true);
	ui->labelDataCopy->setVisible(true);

	ui->labelDataFound->setVisible(false);

	if(hasVCMIBuilderScript)
	{
		ui->pushButtonDataHelp->setVisible(true);
		ui->labelDataHelp->setVisible(true);
	}
}

void FirstLaunchView::heroesDataDetected()
{
	QPalette newPalette = palette();
	newPalette.setColor(QPalette::Base, QColor(50, 200, 50));
	ui->lineEditDataSystem->setPalette(newPalette);
	ui->lineEditDataUser->setPalette(newPalette);

	ui->pushButtonDataSearch->setVisible(false);
	ui->pushButtonDataCopy->setVisible(false);

	ui->labelDataSearch->setVisible(false);
	ui->labelDataCopy->setVisible(false);

	if(hasVCMIBuilderScript)
	{
		ui->pushButtonDataHelp->setVisible(false);
		ui->labelDataHelp->setVisible(false);
	}

	ui->labelDataFound->setVisible(true);

	heroesLanguageUpdate();
}

// Tab Heroes III Data
bool FirstLaunchView::heroesDataDetect()
{
	// user might have copied files to one of our data path.
	// perform full reinitialization of virtual filesystem
	CResourceHandler::destroy();
	CResourceHandler::initialize();
	CResourceHandler::load("config/filesystem.json");

	// use file from lod archive to check presence of H3 data. Very rough estimate, but will work in majority of cases
	bool heroesDataFound = CResourceHandler::get()->existsResource(ResourceID("DATA/GENRLTXT.TXT"));

	return heroesDataFound;
}

void FirstLaunchView::heroesLanguageUpdate()
{
	Languages::fillLanguages(ui->comboBoxLanguage, true);

	QString language = Languages::getHeroesDataLanguage();
	bool success = !language.isEmpty();

	ui->labelDataFailure->setVisible(!success);
	ui->labelDataSuccess->setVisible(success);
	ui->pushButtonDataNext->setEnabled(success);
}

void FirstLaunchView::forceHeroesLanguage(const QString & language)
{
	Settings node = settings.write["general"]["gameDataLanguage"];

	node->String() = language.toStdString();
}

void FirstLaunchView::copyHeroesData()
{
	QDir sourceRoot = QFileDialog::getExistingDirectory(this, "", "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if(!sourceRoot.exists())
		return;

	QStringList dirData = sourceRoot.entryList({"data"}, QDir::Filter::Dirs);
	QStringList dirMaps = sourceRoot.entryList({"maps"}, QDir::Filter::Dirs);
	QStringList dirMp3 = sourceRoot.entryList({"mp3"}, QDir::Filter::Dirs);

	if(dirData.empty() || dirMaps.empty() || dirMp3.empty())
		return;

	QDir sourceData = sourceRoot.filePath(dirData.front());
	QStringList lodArchives = sourceData.entryList({"*.lod"}, QDir::Filter::Files);

	if(lodArchives.empty())
		return;

	QStringList copyDirectories = {dirData.front(), dirMaps.front(), dirMp3.front()};

	QDir targetRoot = pathToQString(VCMIDirs::get().userDataPath());

	for(const QString & dirName : copyDirectories)
	{
		QDir sourceDir = sourceRoot.filePath(dirName);
		QDir targetDir = targetRoot.filePath(dirName);

		if(!targetRoot.exists(dirName))
			targetRoot.mkdir(dirName);

		for(const QString & filename : sourceDir.entryList(QDir::Filter::Files))
		{
			QFile sourceFile(sourceDir.filePath(filename));
			sourceFile.copy(targetDir.filePath(filename));
		}
	}

	heroesDataUpdate();
}

// Tab Mod Preset
void FirstLaunchView::modPresetUpdate()
{
	bool translationExists = !findTranslationModName().isEmpty();

	ui->labelPresetLanguage->setVisible(translationExists);
	ui->checkBoxPresetLanguage->setVisible(translationExists);

	ui->checkBoxPresetLanguage->setEnabled(checkCanInstallTranslation());
	ui->checkBoxPresetExtras->setEnabled(checkCanInstallExtras());
	ui->checkBoxPresetHota->setEnabled(checkCanInstallHota());
	ui->checkBoxPresetWog->setEnabled(checkCanInstallWog());
}

QString FirstLaunchView::findTranslationModName()
{
	if (!getModView())
		return QString();

	QString preferredlanguage = QString::fromStdString(settings["general"]["language"].String());
	QString installedlanguage = QString::fromStdString(settings["session"]["language"].String());

	if (preferredlanguage == installedlanguage)
		return QString();

	return getModView()->getTranslationModName(preferredlanguage);
}

bool FirstLaunchView::checkCanInstallTranslation()
{
	QString modName = findTranslationModName();

	if(modName.isEmpty())
		return false;

	return checkCanInstallMod(modName);
}

bool FirstLaunchView::checkCanInstallWog()
{
	return checkCanInstallMod("wake-of-gods");
}

bool FirstLaunchView::checkCanInstallHota()
{
	return checkCanInstallMod("hota");
}

bool FirstLaunchView::checkCanInstallExtras()
{
	return checkCanInstallMod("vcmi-extras");
}

CModListView * FirstLaunchView::getModView()
{
	auto * mainWindow = dynamic_cast<MainWindow *>(qApp->activeWindow());

	assert(mainWindow);
	if (!mainWindow)
		return nullptr;

	return mainWindow->getModView();
}

bool FirstLaunchView::checkCanInstallMod(const QString & modID)
{
	return getModView() && !getModView()->isModInstalled(modID);
}

void FirstLaunchView::on_pushButtonPresetBack_clicked()
{
	activateTabHeroesData();
}

void FirstLaunchView::on_pushButtonPresetNext_clicked()
{
	QStringList modsToInstall;

	if (ui->checkBoxPresetLanguage && checkCanInstallTranslation())
		modsToInstall.push_back(findTranslationModName());

	if (ui->checkBoxPresetExtras && checkCanInstallExtras())
		modsToInstall.push_back("vcmi-extras");

	if (ui->checkBoxPresetWog && checkCanInstallWog())
		modsToInstall.push_back("wake-of-gods");

	if (ui->checkBoxPresetHota && checkCanInstallHota())
		modsToInstall.push_back("hota");

	exitSetup();

	for (auto const & modName : modsToInstall)
		getModView()->doInstallMod(modName);
}

