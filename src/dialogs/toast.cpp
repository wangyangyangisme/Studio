#include "toast.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QStyle>

#include <QDebug>

Toast::Toast(QWidget *parent) : QWidget(parent)
{
	setParent(parent);
	setObjectName("Toast");
	toastLayout = new QVBoxLayout;
	setLayout(toastLayout);
	caption = new QLabel;
	caption->setObjectName("Caption");
	info = new QLabel;
	info->setObjectName("Info");
	toastLayout->addWidget(caption);
	toastLayout->addWidget(info);

	setWindowFlags(
		Qt::Window						| // Add if popup doesn't show up
		Qt::FramelessWindowHint			| // No window border
		Qt::WindowDoesNotAcceptFocus	| // No focus
		Qt::WindowStaysOnTopHint		  // Always on top
	);

	setStyleSheet(
		"QWidget#Toast { background: #1E1E1E; border: 1px solid black; }"
		"QLabel { color: #EEE; }"
		"QLabel#Caption { font-style: bold; }"
	);

	//setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	//setGeometry(QStyle::alignedRect(Qt::RightToLeft, Qt::AlignTop, size(), QDesktopWidget().availableGeometry()));

	//QRect position = frameGeometry();
	//position.moveCenter(parent->rect().center());
	//move(position.topLeft());


	setAttribute(Qt::WA_ShowWithoutActivating);
	//setGeometry(QStyle::alignedRect(
	//	Qt::LeftToRight,
	//	Qt::AlignTop,
	//	size(),
	//	parent->rect())
	//);
		//qApp->desktop()->availableGeometry()));
}

void Toast::showToast(const QString &title, const QString &text, const float &delay)
{
	caption->setText(title);
	info->setText(text);
	show();
}

void Toast::showToast(const QString &title, const QString &text, const float &delay, const QPoint &pos, const QRect &rect)
{
	caption->setText(title);
	info->setText(text);


	//QRect position = frameGeometry();
	//position.moveBottomRight(QPoint(rect.width() - position.width(), rect.height() - position.height()));
	//move(position.bottomRight());

	setGeometry(QStyle::alignedRect(Qt::RightToLeft, Qt::AlignBottom, QSize(frameGeometry().width(), frameGeometry().height()), rect));

	show();
}
