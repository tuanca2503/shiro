from fastapi import FastAPI
from transformers import AutoModelForCausalLM, AutoTokenizer
import torch
from pydantic import BaseModel

app = FastAPI()

model_name = "./models/base/smollm3-3b"
tokenizer = AutoTokenizer.from_pretrained(model_name)
model = AutoModelForCausalLM.from_pretrained(
    model_name, torch_dtype=torch.bfloat16, device_map="auto"
)
model.eval()


class ChatRequest(BaseModel):
    message: str


@app.post("/chat")
def chat(request: ChatRequest):
    messages = [{"role": "user", "content": request.message}]
    inputs = tokenizer.apply_chat_template(
        messages,
        return_tensors="pt",
        return_dict=True,  # ép trả về dict rõ ràng
        add_generation_prompt=True,  # thêm token báo hiệu "tới lượt assistant trả lời"
    ).to(model.device)
    output = model.generate(
        **inputs, max_new_tokens=200
    )  # unpack bằng ** thay vì truyền thẳng
    response = tokenizer.decode(
        output[0][
            inputs["input_ids"].shape[-1] :
        ],  # chỉ decode phần MỚI sinh ra, bỏ phần prompt
        skip_special_tokens=True,
    )
    return {"response": response}


# uvicorn main:app --host 0.0.0.0 --port 8000


