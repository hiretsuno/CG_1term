#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <WindowsX.h>  // Добавьте эту строку для GET_X_LPARAM/GET_Y_LPARAM

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct Vertex
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;      // нормали
    XMFLOAT2 TexCoord;    // текстурные координаты
    XMFLOAT4 Color;       // цвет
};

struct ObjectConstants
{
    XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

class BoxApp : public D3DApp
{
public:
    BoxApp(HINSTANCE hInstance);
    BoxApp(const BoxApp& rhs) = delete;
    BoxApp& operator=(const BoxApp& rhs) = delete;
    ~BoxApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    // Переопределяем обработчик сообщений
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)override;

    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildSponzaGeometry();
    void BuildPSO();

    // Методы для управления камерой
    void UpdateCamera(const GameTimer& gt);
    void ProcessKeyboardInput(const GameTimer& gt);

private:

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    ComPtr<ID3D12PipelineState> mPSO = nullptr;

    XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    // Камера для свободного полета
    XMFLOAT3 mEyePos = { 0.0f, 2.0f, -15.0f };  // Позиция камеры
    XMFLOAT3 mLookAt = { 0.0f, 0.0f, 0.0f };    // Точка, куда смотрим
    XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };        // Вектор "вверх"

    // Углы камеры (для вращения мышкой)
    float mPitch = 0.0f;    // Вращение вверх/вниз
    float mYaw = 0.0f;      // Вращение влево/вправо

    // Скорость движения камеры
    float mMoveSpeed = 10.0f;
    float mMouseSensitivity = 0.25f;

    // Состояние клавиш
    bool mKeys[256] = { false };

    POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        BoxApp theApp(hInstance);
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

BoxApp::BoxApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
    // Инициализируем состояние клавиш
    memset(mKeys, 0, sizeof(mKeys));
}

BoxApp::~BoxApp()
{
}

bool BoxApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildSponzaGeometry();
    BuildPSO();

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();

    return true;
}

void BoxApp::OnResize()
{
    D3DApp::OnResize();

    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 0.1f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

LRESULT BoxApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Обрабатываем сообщения клавиатуры
    switch (msg)
    {
    case WM_KEYDOWN:
        if (wParam < 256)
        {
            mKeys[wParam] = true;

            // Дополнительные клавиши
            switch (wParam)
            {
            case VK_ESCAPE:
                PostQuitMessage(0);
                break;
            case 'R':  // Сброс камеры
                mEyePos = XMFLOAT3(0.0f, 2.0f, -15.0f);
                mPitch = 0.0f;
                mYaw = 0.0f;
                break;
            case '1':  // Уменьшить скорость
                mMoveSpeed = MathHelper::Max(1.0f, mMoveSpeed - 5.0f);
                break;
            case '2':  // Увеличить скорость
                mMoveSpeed += 5.0f;
                break;
            }
        }
        break;

    case WM_KEYUP:
        if (wParam < 256)
        {
            mKeys[wParam] = false;
        }
        break;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    }

    // Вызываем базовую реализацию для остальных сообщений
    return D3DApp::MsgProc(hwnd, msg, wParam, lParam);
}

void BoxApp::UpdateCamera(const GameTimer& gt)
{
    // Конвертируем углы Эйлера в вектор направления
    float cosPitch = cosf(mPitch);
    float sinPitch = sinf(mPitch);
    float cosYaw = cosf(mYaw);
    float sinYaw = sinf(mYaw);

    // Вычисляем вектор направления камеры
    XMFLOAT3 lookDirection;
    lookDirection.x = cosPitch * sinYaw;
    lookDirection.y = sinPitch;
    lookDirection.z = cosPitch * cosYaw;

    // Нормализуем вектор направления
    XMVECTOR lookDir = XMLoadFloat3(&lookDirection);
    lookDir = XMVector3Normalize(lookDir);

    // Вычисляем правый и верхний векторы
    XMVECTOR up = XMLoadFloat3(&mUp);
    XMVECTOR right = XMVector3Cross(up, lookDir);
    right = XMVector3Normalize(right);

    // Обновляем lookAt точку
    XMVECTOR eyePos = XMLoadFloat3(&mEyePos);
    XMVECTOR lookAt = eyePos + lookDir;
    XMStoreFloat3(&mLookAt, lookAt);

    // Создаем матрицу вида
    XMMATRIX view = XMMatrixLookAtLH(eyePos, lookAt, up);
    XMStoreFloat4x4(&mView, view);
}

void BoxApp::ProcessKeyboardInput(const GameTimer& gt)
{
    float dt = gt.DeltaTime();

    // Вычисляем векторы направления
    XMVECTOR forward = XMLoadFloat3(&mLookAt) - XMLoadFloat3(&mEyePos);
    forward = XMVector3Normalize(forward);
    forward = XMVectorSetY(forward, 0.0f); // Игнорируем вертикальную компоненту для движения вперед/назад
    forward = XMVector3Normalize(forward);

    XMVECTOR right = XMVector3Cross(XMLoadFloat3(&mUp), forward);
    right = XMVector3Normalize(right);

    XMVECTOR up = XMLoadFloat3(&mUp);

    // Скорость движения с учетом времени
    float speed = mMoveSpeed * dt;

    // Обрабатываем движение
    XMVECTOR moveDir = XMVectorZero();

    if (mKeys['W'] || mKeys[VK_UP])       // Вперед
        moveDir += forward * speed;
    if (mKeys['S'] || mKeys[VK_DOWN])     // Назад
        moveDir -= forward * speed;
    if (mKeys['A'] || mKeys[VK_LEFT])     // Влево
        moveDir -= right * speed;
    if (mKeys['D'] || mKeys[VK_RIGHT])    // Вправо
        moveDir += right * speed;
    if (mKeys['E'] || mKeys[' '])         // Вверх (Space или E)
        moveDir += up * speed;
    if (mKeys['Q'] || mKeys['C'])         // Вниз (Q или C)
        moveDir -= up * speed;

    // Применяем движение
    XMVECTOR eyePos = XMLoadFloat3(&mEyePos);
    eyePos += moveDir;
    XMStoreFloat3(&mEyePos, eyePos);

    // Обновляем точку, куда смотрим
    XMVECTOR lookAt = eyePos + forward;
    XMStoreFloat3(&mLookAt, lookAt);
}

void BoxApp::Update(const GameTimer& gt)
{
    // Обрабатываем ввод с клавиатуры
    ProcessKeyboardInput(gt);

    // Обновляем камеру
    UpdateCamera(gt);

    XMMATRIX world = XMLoadFloat4x4(&mWorld);

    // Масштабируем Sponza (обычно она большая)
    world = XMMatrixScaling(0.01f, 0.01f, 0.01f);

    XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    XMMATRIX worldViewProj = world * view * proj;

    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
    mObjectCB->CopyData(0, objConstants);
}

void BoxApp::Draw(const GameTimer& gt)
{
    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    mCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->VertexBufferView());
    mCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
    mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

    mCommandList->DrawIndexedInstanced(
        mBoxGeo->DrawArgs["sponza"].IndexCount,
        1, 0, 0, 0);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    FlushCommandQueue();
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        // Вычисляем смещение мыши
        float dx = XMConvertToRadians(mMouseSensitivity * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(mMouseSensitivity * static_cast<float>(y - mLastMousePos.y));

        // Обновляем углы камеры
        mYaw += dx;
        mPitch += dy;

        // Ограничиваем угол pitch, чтобы не перевернуть камеру
        const float maxPitch = XM_PIDIV2 - 0.01f;
        mPitch = MathHelper::Clamp(mPitch, -maxPitch, maxPitch);

        // Нормализуем yaw в диапазоне [0, 2π]
        if (mYaw > XM_2PI)
            mYaw -= XM_2PI;
        else if (mYaw < 0)
            mYaw += XM_2PI;
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void BoxApp::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&mCbvHeap)));
}

void BoxApp::BuildConstantBuffers()
{
    mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
    // Offset to the ith object constant buffer in the buffer.
    int boxCBufIndex = 0;
    cbAddress += boxCBufIndex * objCBByteSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    md3dDevice->CreateConstantBufferView(
        &cbvDesc,
        mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void BoxApp::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    // Create a single descriptor table of CBVs.
    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature)));
}

void BoxApp::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;

    // Используйте шейдеры, которые поддерживают нормали и текстуры
    mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
    mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

    // Обновленный input layout
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void BoxApp::BuildSponzaGeometry()
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices; // Используем uint32_t для больших моделей

    // Пример пути к файлу Sponza
    // Варианты расположения файла:
    // 1. В папке с исполняемым файлом: "sponza.obj"
    // 2. В папке Models: "Models/sponza.obj"
    // 3. Абсолютный путь: "C:/Models/sponza.obj"

    // Выберите подходящий путь для вашего проекта:
    std::string filename = "sponza.obj";  // Файл в той же папке что и exe
    // Или: std::string filename = "Models/Sponza/sponza.obj";
    // Или: std::string filename = "../Models/sponza.obj";

    // Загрузка OBJ файла
    std::ifstream file(filename);
    if (!file.is_open())
    {
        // Попробуем альтернативные пути
        std::vector<std::string> possiblePaths = {
            "sponza.obj",
            "Models/sponza.obj",
            "../Models/sponza.obj",
            "../../Models/sponza.obj",
            "Sponza/sponza.obj",
            "../Sponza/sponza.obj"
        };

        bool opened = false;
        for (const auto& path : possiblePaths)
        {
            file.open(path);
            if (file.is_open())
            {
                filename = path;
                opened = true;
                break;
            }
        }

        if (!opened)
        {
            std::wstring message = L"Не удалось открыть файл sponza.obj. Искали по путям:\n";
            for (const auto& path : possiblePaths)
            {
                message += std::wstring(path.begin(), path.end()) + L"\n";
            }
            MessageBox(nullptr, message.c_str(), L"Ошибка", MB_OK);
            return;
        }
    }

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;
    std::vector<XMFLOAT2> texcoords;

    // Для индексов используем векторы
    std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.substr(0, 2) == "v ")
        {
            std::istringstream iss(line.substr(2));
            XMFLOAT3 vertex;
            iss >> vertex.x >> vertex.y >> vertex.z;
            positions.push_back(vertex);
        }
        else if (line.substr(0, 3) == "vt ")
        {
            std::istringstream iss(line.substr(3));
            XMFLOAT2 uv;
            iss >> uv.x >> uv.y;
            uv.y = 1.0f - uv.y; // Инвертируем V координату для DirectX
            texcoords.push_back(uv);
        }
        else if (line.substr(0, 3) == "vn ")
        {
            std::istringstream iss(line.substr(3));
            XMFLOAT3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }
        else if (line.substr(0, 2) == "f ")
        {
            std::istringstream iss(line.substr(2));
            std::string vertex1, vertex2, vertex3;
            iss >> vertex1 >> vertex2 >> vertex3;

            unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];

            // Парсим вершины грани
            std::string vertices[3] = { vertex1, vertex2, vertex3 };

            for (int i = 0; i < 3; i++)
            {
                std::stringstream vss(vertices[i]);
                std::string part;
                std::vector<std::string> parts;

                while (std::getline(vss, part, '/'))
                {
                    parts.push_back(part);
                }

                if (parts.size() >= 1 && !parts[0].empty())
                    vertexIndex[i] = std::stoi(parts[0]) - 1;
                if (parts.size() >= 2 && !parts[1].empty())
                    uvIndex[i] = std::stoi(parts[1]) - 1;
                if (parts.size() >= 3 && !parts[2].empty())
                    normalIndex[i] = std::stoi(parts[2]) - 1;

                vertexIndices.push_back(vertexIndex[i]);
                if (parts.size() >= 2 && !parts[1].empty())
                    uvIndices.push_back(uvIndex[i]);
                if (parts.size() >= 3 && !parts[2].empty())
                    normalIndices.push_back(normalIndex[i]);
            }
        }
    }

    file.close();

    // Выводим информацию о загруженной модели
    OutputDebugStringA(("Загружено: " + std::to_string(positions.size()) + " вершин, " +
        std::to_string(vertexIndices.size() / 3) + " треугольников\n").c_str());

    // Создаем вершины
    for (size_t i = 0; i < vertexIndices.size(); i++)
    {
        Vertex vertex;

        // Позиция
        if (vertexIndices[i] < positions.size())
            vertex.Pos = positions[vertexIndices[i]];
        else
            vertex.Pos = XMFLOAT3(0.0f, 0.0f, 0.0f);

        // Нормаль
        if (i < normalIndices.size() && normalIndices[i] < normals.size())
            vertex.Normal = normals[normalIndices[i]];
        else
            vertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

        // Текстурные координаты
        if (i < uvIndices.size() && uvIndices[i] < texcoords.size())
            vertex.TexCoord = texcoords[uvIndices[i]];
        else
            vertex.TexCoord = XMFLOAT2(0.0f, 0.0f);

        // Цвет (можно настроить как вам нужно)
        float gray = 0.7f;
        vertex.Color = XMFLOAT4(gray, gray, gray, 1.0f);

        vertices.push_back(vertex);
        indices.push_back(static_cast<uint32_t>(i));
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(uint32_t);

    mBoxGeo = std::make_unique<MeshGeometry>();
    mBoxGeo->Name = "sponzaGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
    CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
    CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, mBoxGeo->VertexBufferUploader);

    mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, mBoxGeo->IndexBufferUploader);

    mBoxGeo->VertexByteStride = sizeof(Vertex);
    mBoxGeo->VertexBufferByteSize = vbByteSize;
    mBoxGeo->IndexFormat = DXGI_FORMAT_R32_UINT; // Изменено на 32-битный
    mBoxGeo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    mBoxGeo->DrawArgs["sponza"] = submesh;
}

void BoxApp::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
        mvsByteCode->GetBufferSize()
    };
    psoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
        mpsByteCode->GetBufferSize()
    };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}