#include "qboxoutput.h"
#include "qboxserver.h"

QBoxOutPut::QBoxOutPut(QBoxServer *server):
    m_server(server),
    QObject(server)
{
    outputLayout = new QWOutputLayout(this);
    connect(m_server->backend, &QWBackend::newOutput, this, &QBoxOutPut::onNewOutput);
}

void QBoxOutPut::onNewOutput(QWOutput *output)
{
    Q_ASSERT(output);
    outputs.append(output);

    output->initRender(m_server->allocator, m_server->renderer);
    if (!wl_list_empty(&output->handle()->modes)) {
        auto *mode = output->preferredMode();
        output->setMode(mode);
        output->enable(true);
        if (!output->commit())
            return;
    }

    connect(output, &QWOutput::frame, this, &QBoxOutPut::onOutputFrame);
    outputLayout->addAuto(output->handle());
}

void QBoxOutPut::onOutputFrame()
{
    auto output = qobject_cast<QWOutput*>(sender());
    Q_ASSERT(output);
    auto sceneOutput = QWSceneOutput::from(m_server->xdgShell->getScene(), output);
    sceneOutput->commit();

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    sceneOutput->sendFrameDone(&now);
}
